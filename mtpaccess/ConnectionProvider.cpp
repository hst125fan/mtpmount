#include "../dokanconnect/MemLeakDetection.h"
#include "ConnectionProvider.h"

#include "TestConnection.h"
#include "MtpTransfer.h"

MtpConnectionProvider& MtpConnectionProvider::getInstance()
{
	if (_instance == nullptr)
	{
		_instance = new MtpConnectionProvider;
	}
	return *_instance;
}

MtpConnectionProvider* MtpConnectionProvider::_instance = nullptr;

MtpConnectionProvider::MtpConnectionProvider()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr))
	{
		throw(std::exception("CoInitializeEx failed"));
	}

	hr = CoCreateInstance(CLSID_PortableDeviceManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_portableDeviceManager));
	if (FAILED(hr))
	{
		throw(std::exception("CoCreateInstance (_portableDeviceManager) failed"));
	}

	if (!_refreshDeviceList(false))
	{
		throw(std::exception("_refreshDeviceList failed"));
	}
}

bool MtpConnectionProvider::_refreshDeviceList(bool fullRefresh)
{
	if (fullRefresh)
	{
		HRESULT hr = _portableDeviceManager->RefreshDeviceList();
		if (FAILED(hr))
		{
			return false;
		}
	}
	DWORD howManyDevicesHaveIGot = 0;
	HRESULT hr = _portableDeviceManager->GetDevices(NULL, &howManyDevicesHaveIGot);
	if (FAILED(hr))
	{
		return false;
	}

#ifndef DBG_TEST_VIRTUAL_FILESYSTEM
	if (howManyDevicesHaveIGot == 0) { return true; }
#endif

	LPWSTR* deviceIDs = new LPWSTR[howManyDevicesHaveIGot];
	hr = _portableDeviceManager->GetDevices(deviceIDs, &howManyDevicesHaveIGot);

	if (SUCCEEDED(hr))
	{
		_availableDeviceGUIDs.clear();
		_availableDeviceGUIDs.reserve(howManyDevicesHaveIGot);
	}
	for (DWORD i = 0; i < howManyDevicesHaveIGot; i++)
	{
		if (SUCCEEDED(hr))
		{
			_availableDeviceGUIDs.emplace_back(deviceIDs[i]);
		}
		//delete deviceIDs[i]; //according to win documentation this should be done, but it simply does not work
	}
#ifdef DBG_TEST_VIRTUAL_FILESYSTEM
	_availableDeviceGUIDs.emplace_back(L"this_is_not_a_device_id");
#endif

	delete deviceIDs;
	return (SUCCEEDED(hr)) ? true : false;
}

size_t MtpConnectionProvider::getDeviceCount()
{
	return _availableDeviceGUIDs.size();
}

bool MtpConnectionProvider::getFriendlyNameOfDevice(int deviceInListId, std::wstring& nameWillBePutHere)
{
	std::wstring deviceGUID;
	try
	{
		deviceGUID = _availableDeviceGUIDs.at(deviceInListId);
	}
	catch (std::out_of_range)
	{
		return false;
	}
#ifdef DBG_TEST_VIRTUAL_FILESYSTEM
	if (deviceGUID == L"this_is_not_a_device_id")
	{
		nameWillBePutHere = L"TestDevice";
		return true;
	}
#endif

	DWORD bufsize = 0;
	HRESULT hr = _portableDeviceManager->GetDeviceFriendlyName(deviceGUID.c_str(), NULL, &bufsize);
	if (FAILED(hr))
	{
		return false;
	}

	LPWSTR writebuf = new WCHAR[bufsize];
	hr = _portableDeviceManager->GetDeviceFriendlyName(deviceGUID.c_str(), writebuf, &bufsize);
	if (SUCCEEDED(hr))
	{
		nameWillBePutHere = writebuf;
	}
	delete writebuf;
	return (SUCCEEDED(hr)) ? true : false;
}

bool MtpConnectionProvider::_getClientInfo(IPortableDeviceValues** vals)
{
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(vals));
	if (SUCCEEDED(hr))
	{
		// Attempt to set all bits of client information
		(*vals)->SetStringValue(WPD_CLIENT_NAME, L"TEST");
		(*vals)->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, 0);
		(*vals)->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, 0);
		(*vals)->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, 0);
		(*vals)->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
		return true;
	}
	else
	{
		return false;
	}
}

bool MtpConnectionProvider::openConnectionToDevice(int deviceInListId, AbstractConnection** openedConnection, bool readonly)
{
	std::wstring devGUID;
	try
	{
		devGUID = _availableDeviceGUIDs.at(deviceInListId);
	}
	catch (std::out_of_range)
	{
		return false;
	}


	try
	{
		(*openedConnection) = _availableConnections.at(devGUID);
		(*openedConnection)->refAdd();
		return true;
	}
	catch (std::out_of_range)
	{
		//continue with the operations below...
	}

#ifdef DBG_TEST_VIRTUAL_FILESYSTEM
	if (devGUID == L"this_is_not_a_device_id")
	{
		AbstractConnection* newConnection = new TestConnection(devGUID);
		_availableConnections.insert(typeof_availableConnections::value_type(devGUID, newConnection));
		(*openedConnection) = newConnection;
		return true;
	}
#endif


	IPortableDevice* dev;
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceFTM, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dev));
	if (FAILED(hr))
	{
		return false;
	}

	IPortableDeviceValues* clientInfo;
	if (!_getClientInfo(&clientInfo))
	{
		return false;
	}

	if (readonly)
	{
		clientInfo->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS, GENERIC_READ);
	}

	hr = dev->Open(devGUID.c_str(), clientInfo);

	if (SUCCEEDED(hr))
	{
		MtpConnection* newConnection = new MtpConnection(dev, devGUID);
		_availableConnections.insert(typeof_availableConnections::value_type(devGUID, newConnection));
		(*openedConnection) = newConnection;
		return true;
	}
	else
	{
		ActuallyDeleteComObject(dev)
		return false;
	}
}

MtpConnectionProvider::~MtpConnectionProvider()
{
	for (typeof_availableConnections::iterator iter = _availableConnections.begin();
		iter != _availableConnections.end();
		iter++)
	{
		delete(iter->second);
	}
	CoUninitialize();
}

void MtpConnectionProvider::closeConnection(AbstractConnection* conn)
{
	conn->refDel();
	if (conn->getRefcount() < 1)
	{
		try
		{
			std::wstring whatIWantToDelete = conn->getDeleteKey();
			delete _availableConnections.at(whatIWantToDelete);
			_availableConnections.erase(whatIWantToDelete);
		}
		catch (std::out_of_range)
		{
			//I'm guessing it's already deleted...
		}
	}
}
