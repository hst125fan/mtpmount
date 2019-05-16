#pragma once

#include <Windows.h>
#include <PortableDevice.h>
#include <PortableDeviceApi.h>
#include <unordered_map>
#include <vector>
#include <string>
#include "AbstractConnection.h"

class MtpConnectionProvider
{
private:
	static MtpConnectionProvider* _instance;

	MtpConnectionProvider();
	MtpConnectionProvider(MtpConnectionProvider const& p) = delete;
	MtpConnectionProvider& operator= (MtpConnectionProvider const& p) = delete;

	IPortableDeviceManager* _portableDeviceManager;
	std::vector<std::wstring> _availableDeviceGUIDs;
	typedef std::unordered_map<std::wstring, AbstractConnection*> typeof_availableConnections;
	typeof_availableConnections _availableConnections;

	bool _getClientInfo(IPortableDeviceValues** vals);
	bool _refreshDeviceList(bool fullRefresh = true);
public:
	static MtpConnectionProvider& getInstance();
	~MtpConnectionProvider();
	static void testReset_DO_NOT_USE() { _instance = NULL; }

	bool refresh() { return _refreshDeviceList(true); }

	//Obtain count of connected and found devices. Note, one device might contain multiple file systems
	//Returns: Count
	size_t getDeviceCount();

	//Obtain the user-friendly name of a device discovered by the device manager.
	//Parameters:
	//(1) IN	int deviceInListId				which device? (0=first, 1=second, ... )
	//(2) OUT	std::wstring& nameWillBePutHere	the user-friendly name of the device
	//Returns: true if succeeded, false on failure (e.g. deviceInListId / index not available )
	bool getFriendlyNameOfDevice(int deviceInListId, std::wstring& nameWillBePutHere);

	//open a connection to a device
	//Parameters:
	//(1) IN	int deviceInListId					which device? (0=first, 1=second, ... )
	//(2) OUT	MtpConnection** openedConnection	pointer to the connection object
	//(3) IN	bool readonly (OPTIONAL)			open connection as read-only
	//Returns: true on success, false on failure
	bool openConnectionToDevice(int deviceInListId, AbstractConnection** openedConnection, bool readonly = false);

	//close a connection. If a connection is used multiple times (openConnectionToDevice called more than once), the connection won't be closed until for every open call a close call occured.
	//Parameters:
	//(1) IN	MtpConnection* conn		pointer to the connection object
	void closeConnection(AbstractConnection* conn);

};