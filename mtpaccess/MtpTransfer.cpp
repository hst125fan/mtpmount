#include "../dokanconnect/MemLeakDetection.h"
#include "MtpTransfer.h"
#include <exception>

#pragma comment( lib, "PortableDeviceGuids.lib" )


MtpConnection::MtpConnection(IPortableDevice* dev, std::wstring& ident) : _dev(dev), AbstractConnection(ident)
{
	_iKnowThatMyDeviceIsEmpty = false;
	_devcont = NULL;
}

IPortableDeviceContent* MtpConnection::getDeviceContentInterface()
{
	if (_devcont == NULL)
	{
		/*HRESULT hr =*/ _dev->Content(&_devcont);
	}
	return _devcont;
}

MtpConnection::~MtpConnection()
{
	for (std::vector<MtpMappableDrive*>::iterator iter = _mappableDrives.begin();
		iter != _mappableDrives.end();
		iter++)
	{
		delete (*iter);
		(*iter) = NULL;
	}
	releaseDeviceContentInterface();
	_dev->Close();
	ActuallyDeleteComObject(_dev)
}

bool MtpConnection::getMappableDriveNames(std::vector<std::wstring>& putNamesHere, bool forceRefresh)
{
	putNamesHere.clear();
	if (_iKnowThatMyDeviceIsEmpty && !forceRefresh) 
	{ 
		return true; 
	}
	else
	{
		bool ret = true;
		if (_mappableDrives.empty())
		{
			ret = _lookForMappableDrives();
		}
		for (std::vector<MtpMappableDrive*>::iterator iter = _mappableDrives.begin();
			iter != _mappableDrives.end();
			iter++)
		{
			putNamesHere.push_back((*iter)->getFriendlyName());
		}
		return ret;
	}
	
}

bool MtpConnection::getMappableDriveCnt(int& putCountHere, bool forceRefresh)
{
	if (_iKnowThatMyDeviceIsEmpty && !forceRefresh) 
	{ 
		putCountHere = 0;
		return true;
	}
	else
	{
		bool ret = true;
		if (_mappableDrives.empty())
		{
			ret = _lookForMappableDrives();
		}
		putCountHere = static_cast<int>(_mappableDrives.size());
		return ret;
	}
}

bool MtpConnection::_lookForMappableDrives()
{
	IPortableDeviceContent* deviceContent = getDeviceContentInterface();

	IEnumPortableDeviceObjectIDs* enumObjectIDs;
	HRESULT hr = deviceContent->EnumObjects(0, WPD_DEVICE_OBJECT_ID, nullptr, &enumObjectIDs);
	if (FAILED(hr))
	{
		releaseDeviceContentInterface();
		return false;
	}

	//Now we have enumerated all objects on layer "DEVICE + 1" (one layer above device root). We expect all storage media to be here!

	HRESULT enumHr = S_OK;
	while (enumHr == S_OK)
	{
		DWORD  numFetched = 0;
		PWSTR  objectID;
		//obtaining always a single object only!
		enumHr = enumObjectIDs->Next(1, &objectID, &numFetched);
		if (SUCCEEDED(enumHr) && numFetched == 1)
		{
			//This code below will be executed for every root-level object we got.

			IPortableDeviceProperties* props;
			
			HRESULT inEnumHr = deviceContent->Properties(&props);
			if (FAILED(inEnumHr))
			{
				continue;
			}

			IPortableDeviceKeyCollection* readprops;
			inEnumHr = CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&readprops));
			if (FAILED(inEnumHr))
			{
				continue;
			}

			readprops->Add(WPD_OBJECT_NAME);
			readprops->Add(WPD_OBJECT_CONTENT_TYPE);
			readprops->Add(WPD_FUNCTIONAL_OBJECT_CATEGORY);

			IPortableDeviceValues* values;
			inEnumHr = props->GetValues(objectID, readprops, &values);
			if (FAILED(inEnumHr))
			{
				continue;
			}
			PWSTR name;
			GUID contTypeGUID;
			GUID funcGUID;
			values->GetStringValue(WPD_OBJECT_NAME, &name);
			values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE, &contTypeGUID);
			values->GetGuidValue(WPD_FUNCTIONAL_OBJECT_CATEGORY, &funcGUID);

			//storage media is categorized as functional object
			if (contTypeGUID == WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT &&
				funcGUID == WPD_FUNCTIONAL_CATEGORY_STORAGE)
			{
				//at this point we know we're having a storage media we can (hopefully) map!
				std::wstring nameAsWstring(name);
				std::wstring idAsWstring(objectID);
				if( ! _mappableDrives.empty() )
				{
					for (std::vector<MtpMappableDrive*>::iterator iter = _mappableDrives.begin();
						iter != _mappableDrives.end();
						iter++)
					{
						if (idAsWstring == (*iter)->_objID)
						{
							continue; //this means object is already in list -> don't need to add it
						}
					}
				}
				MtpMappableDrive* drv = new MtpMappableDrive(nameAsWstring, idAsWstring, this);
				_mappableDrives.push_back(drv);
			}
		}
		else if (numFetched == 0)
		{
			//No more objects - we are finished
			break;
		}
	}

	_iKnowThatMyDeviceIsEmpty = (_mappableDrives.empty()) ? true : false;
	releaseDeviceContentInterface();
	return true;

}

MtpMappableDrive::MtpMappableDrive(std::wstring & friendlyname, std::wstring& objID, MtpConnection* backref) : AbstractMappableDrive( friendlyname ), _objID( objID ), _usedConnection( backref )
{
	driveRoot = new MtpFsDirectory(objID, L"", NULL, _usedConnection);
}

MtpMappableDrive::MtpFsDirectory::~MtpFsDirectory()
{
	for (std::vector<MtpFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		delete (*iter);
		(*iter) = NULL;
	}
}

void MtpMappableDrive::MtpFsDirectory::deleteSubCache()
{
	for (std::vector<MtpFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		delete (*iter);
		(*iter) = NULL;
	}

	_children.clear();
	haveChildrenEvaluated = false;
}

bool DEPRECATED MtpMappableDrive::MtpFsDirectory::getDirContent(std::vector<MtpFsNode*>& putHere, unsigned int from, unsigned int until)
{
	if (!haveChildrenEvaluated)
	{
		_evaluateChildren();
	}

	if (from <= 0 && until >= (_children.size() - 1))
	{
		putHere = _children;
	}
	else
	{
		from = max(from, 0);
		until = min(until,static_cast<unsigned int>(_children.size()-1));
		putHere.reserve(until - from);
		for (unsigned int i = from; i <= until; i++)
		{
			putHere.push_back(_children.at(i));
		}
	}
	
	return true;
}

bool MtpMappableDrive::MtpFsDirectory::_evaluateChildren()
{
	IEnumPortableDeviceObjectIDs* enumObjectIDs;
	//after the !first! object was deleted, this call here (EnumObjects) takes VERY long. When deleting another object, this does not occur!
	HRESULT hr = _usedConnection->getDeviceContentInterface()->EnumObjects(0, _objectID.c_str(), nullptr, &enumObjectIDs);
	if (FAILED(hr))
	{
		_usedConnection->releaseDeviceContentInterface();
		return false;
	}
	
	//got all content. Now put it in vector + all additional info we might need

	HRESULT enumHr = S_OK;
	while (enumHr == S_OK)
	{
		DWORD  numFetched = 0;
		PWSTR  newobjectID;
		//obtaining always a single object only!
		enumHr = enumObjectIDs->Next(1, &newobjectID, &numFetched);
		if (SUCCEEDED(enumHr) && numFetched == 1)
		{
			//This code below will be executed for every object we got.
			MtpCommonObjectProperties props(_usedConnection, newobjectID);
			
			MtpFsNode* newCreatedNode = NULL;
			//okay got everything!
			if (props.getContentType() == WPD_CONTENT_TYPE_FOLDER)
			{
				newCreatedNode = new MtpFsDirectory(newobjectID, std::wstring(props.getName()), this, this->_usedConnection);
			}
			else
			{
				MtpCommonFileOnlyProperties fileonlyprops(_usedConnection, newobjectID);
				newCreatedNode = new MtpFsFile(newobjectID, std::wstring(props.getName()), this, this->_usedConnection, fileonlyprops.getSize());
				
			}

			//additional attributes
			if (props.getDeleteAllowance() == FALSE) { newCreatedNode->disallowDeletion(); }
			newCreatedNode->setCreationTime(props.getCreationDate());
			newCreatedNode->setModificationTime(props.getModificationDate());
			newCreatedNode->setAuthorizationTime(props.getAuthorizationDate());

			_children.push_back(newCreatedNode);
		}
		else if (numFetched == 0)
		{
			//No more objects - we are finished
			break;
		}
	}

	haveChildrenEvaluated = true;
	_usedConnection->releaseDeviceContentInterface();
	return true;
}

AbstractMappableDrive* MtpConnection::getMDriveByFName(std::wstring friendlyname)
{
	for (std::vector<MtpMappableDrive*>::iterator iter = _mappableDrives.begin();
		iter != _mappableDrives.end();
		iter++)
	{
		if ((*iter)->getFriendlyName() == friendlyname)
		{
			return (*iter);
		}
	}
	return NULL;
}
 
MtpFsNode* MtpMappableDrive::MtpFsDirectory::getContentByName(CaseInsensitiveWstring subdirName)
{
	if (!haveChildrenEvaluated)
	{
		_evaluateChildren();
	}
	
	for (std::vector<MtpFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if (CaseInsensitiveWstring((*iter)->getNodeName()) == subdirName)
		{
			return (*iter);
		}
	}
	return NULL;
}

bool MtpMappableDrive::MtpFsFile::contentToStream(std::ostream& stream)
{
	IPortableDeviceResources* resources;
	HRESULT hr = _usedConnection->getDeviceContentInterface()->Transfer(&resources);
	if (FAILED(hr))
	{
		_usedConnection->releaseDeviceContentInterface();
		return false;
	}

	IStream* wpdStream;
	DWORD optimalTransferSize;
	hr = resources->GetStream(_objectID.c_str(), WPD_RESOURCE_DEFAULT, STGM_READ, &optimalTransferSize, &wpdStream);
	if (FAILED(hr))
	{
		_usedConnection->releaseDeviceContentInterface();
		return false;
	}

	ULONG actuallyReadBytes = 1;
	while (actuallyReadBytes > 0)
	{
		char* putHere = new char[optimalTransferSize];
		wpdStream->Read(putHere, optimalTransferSize, &actuallyReadBytes);
		stream.write(putHere, actuallyReadBytes); //does this really work for binary (=0x00 bytes)?
		delete putHere;
	}
	ActuallyDeleteComObject( wpdStream )
	_usedConnection->releaseDeviceContentInterface();
	return true;
}

AbstractFsNode* MtpMappableDrive::_filePathToNode(FsPath& path)
{
	AbstractFsNode* currentNode = driveRoot;
	for (int i = 0; i < path.getPathLength(); i++)
	{
		currentNode = currentNode->getContentByName(path.getPathOnLayer(i));
		if (currentNode == NULL) { return NULL; }
	}
	return currentNode;
}

bool MtpMappableDrive::MtpFsDirectory::createNewFileWithStream(CaseInsensitiveWstring name, std::istream& stream, unsigned long long filesize, FILETIME modified, FILETIME created, FILETIME authored)
{
	//get the properties for the new-to-create file
	IPortableDeviceValues* objectProperties;
	if( ! _fillPropertiesForNewFile(name.getActualCase(), &objectProperties, filesize, created, modified, authored) )
	{
		return false;
	}

	//Create new file and obtain its IStream:
	IStream* wpdStream;
	DWORD optimalBufSize;
	HRESULT hr = _usedConnection->getDeviceContentInterface()->CreateObjectWithPropertiesAndData(objectProperties, &wpdStream, &optimalBufSize, nullptr);
	if (FAILED(hr))
	{
		_usedConnection->releaseDeviceContentInterface();
		return false;
	}

	//Transfer data using IStream
	std::streamsize actuallyRead = 1;
	while (actuallyRead > 0)
	{
		char* buf = new char[optimalBufSize];
		stream.read(buf, optimalBufSize);
		actuallyRead = stream.gcount();

		DWORD actuallyWritten = 0;
		HRESULT writeHr = wpdStream->Write((void*)buf, (ULONG)actuallyRead, &actuallyWritten);

		delete buf;
		if (actuallyWritten != actuallyRead || FAILED(writeHr))
		{
			_usedConnection->releaseDeviceContentInterface();
			return false;
		}
	}

	//Commit changes -> this will actually cause the connected device to save our data
	hr = wpdStream->Commit(STGC_DEFAULT);
	ActuallyDeleteComObject(wpdStream)
	_usedConnection->releaseDeviceContentInterface();
	return (FAILED(hr)) ? false : true;
}

bool MtpMappableDrive::MtpFsDirectory::_fillPropertiesForNewFile(std::wstring name, IPortableDeviceValues** values, unsigned long long filesize, FILETIME creationTime, FILETIME modifyTime, FILETIME authortime)
{
	//First, create the values object
	*values = nullptr;
	IPortableDeviceValues* tempValues;
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tempValues));
	if (FAILED(hr))
	{
		return false;
	}

	//Get all the properties we need for all files
	//parent folder
	hr = tempValues->SetStringValue(WPD_OBJECT_PARENT_ID, _objectID.c_str() );
	if (FAILED(hr))
	{
		return false;
	}
	//file size
	hr = tempValues->SetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE, filesize);
	if (FAILED(hr))
	{
		return false;
	}
	//file name
	hr = tempValues->SetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, name.c_str() );
	if (FAILED(hr))
	{
		return false;
	}
	//file name without extension (which seriously nobody needs, but ... go nuts... )
	hr = tempValues->SetStringValue(WPD_OBJECT_NAME, Utils::getFileNameWithoutEnding(name).c_str() );
	if (FAILED(hr))
	{
		return false;
	}
	//modification time - only modification time is used! - this does not really work
	if (modifyTime.dwHighDateTime != 0 || modifyTime.dwLowDateTime != 0)
	{
		SYSTEMTIME intermediate;
		PROPVARIANT variant;
		PropVariantInit(&variant);
		variant.vt = VT_DATE;
		FileTimeToSystemTime(&modifyTime, &intermediate);
		SystemTimeToVariantTime(&intermediate, &variant.date);
		HRESULT check = tempValues->SetValue(WPD_OBJECT_DATE_MODIFIED, &variant);
		PropVariantClear(&variant);
	}

	if (creationTime.dwHighDateTime != 0 || creationTime.dwLowDateTime != 0)
	{
		SYSTEMTIME intermediate;
		PROPVARIANT variant;
		PropVariantInit(&variant);
		variant.vt = VT_DATE;
		FileTimeToSystemTime(&creationTime, &intermediate);
		SystemTimeToVariantTime(&intermediate, &variant.date);
		HRESULT check = tempValues->SetValue(WPD_OBJECT_DATE_CREATED, &variant);
		PropVariantClear(&variant);
	}

	if (authortime.dwHighDateTime != 0 || authortime.dwLowDateTime != 0)
	{
		SYSTEMTIME intermediate;
		PROPVARIANT variant;
		PropVariantInit(&variant);
		variant.vt = VT_DATE;
		FileTimeToSystemTime(&authortime, &intermediate);
		SystemTimeToVariantTime(&intermediate, &variant.date);
		HRESULT check = tempValues->SetValue(WPD_OBJECT_DATE_AUTHORED, &variant);
		PropVariantClear(&variant);
	}


	//Now specify the file type to set values in more detail

	switch (Utils::classifyFileByName(name))
	{
		case Utils::FILETYPE_MUSIC :
		{
			HRESULT hr = tempValues->SetGuidValue(WPD_OBJECT_CONTENT_TYPE, WPD_CONTENT_TYPE_AUDIO);
			if (FAILED(hr))
			{
				return false;
			}
			hr = tempValues->SetGuidValue(WPD_OBJECT_FORMAT, WPD_OBJECT_FORMAT_MP3);
			if (FAILED(hr))
			{
				return false;
			}
			break;
		}
		case Utils::FILETYPE_IMAGE :
		{
			HRESULT hr = tempValues->SetGuidValue(WPD_OBJECT_CONTENT_TYPE, WPD_CONTENT_TYPE_IMAGE);
			if (FAILED(hr))
			{
				return false;
			}
			hr = tempValues->SetGuidValue(WPD_OBJECT_FORMAT, WPD_OBJECT_FORMAT_EXIF);
			if (FAILED(hr))
			{
				return false;
			}
			break;
		}
		case Utils::FILETYPE_CONTACT :
		{
			HRESULT hr = tempValues->SetGuidValue(WPD_OBJECT_CONTENT_TYPE, WPD_CONTENT_TYPE_CONTACT);
			if (FAILED(hr))
			{
				return false;
			}
			hr = tempValues->SetGuidValue(WPD_OBJECT_FORMAT, WPD_OBJECT_FORMAT_VCARD2);
			if (FAILED(hr))
			{
				return false;
			}
			break;
		}
	}

	//ok got everything - caller can have our list
	*values = tempValues;
	return true;
}

//Modification date cannot be set in android devices!
void MtpMappableDrive::MtpFsFile::syncFileTime(FILETIME m, FILETIME c, FILETIME a)
{
	_timeModified = m;
	_timeAuthored = a;
	_timeCreated = c;
	IPortableDeviceValues* values;
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&values));
	if (m.dwHighDateTime != 0 || m.dwLowDateTime != 0)
	{
		SYSTEMTIME intermediate;
		PROPVARIANT variant;
		PropVariantInit(&variant);
		variant.vt = VT_DATE;
		FileTimeToSystemTime(&m, &intermediate);
		SystemTimeToVariantTime(&intermediate, &variant.date);
		HRESULT check = values->SetValue(WPD_OBJECT_DATE_MODIFIED, &variant);
		PropVariantClear(&variant);
	}
	if (c.dwHighDateTime != 0 || c.dwLowDateTime != 0)
	{
		SYSTEMTIME intermediate;
		PROPVARIANT variant;
		PropVariantInit(&variant);
		variant.vt = VT_DATE;
		FileTimeToSystemTime(&c, &intermediate);
		SystemTimeToVariantTime(&intermediate, &variant.date);
		HRESULT check = values->SetValue(WPD_OBJECT_DATE_CREATED, &variant);
		PropVariantClear(&variant);
	}
	if (a.dwHighDateTime != 0 || a.dwLowDateTime != 0)
	{
		SYSTEMTIME intermediate;
		PROPVARIANT variant;
		PropVariantInit(&variant);
		variant.vt = VT_DATE;
		FileTimeToSystemTime(&a, &intermediate);
		SystemTimeToVariantTime(&intermediate, &variant.date);
		HRESULT check = values->SetValue(WPD_OBJECT_DATE_AUTHORED, &variant);
		PropVariantClear(&variant);
	}

	IPortableDeviceProperties* devprops;
	IPortableDeviceValues* results;
	HRESULT hrcheck = _usedConnection->getDeviceContentInterface()->Properties(&devprops);
	if (FAILED(hr)) { _usedConnection->releaseDeviceContentInterface(); return; }
	devprops->SetValues(_objectID.c_str(), values, &results);
	_usedConnection->releaseDeviceContentInterface();
}

bool MtpMappableDrive::MtpFsFile::deleteMe()
{
	//the "delete" method wants a collection, even if there's only one object.
	IPortableDevicePropVariantCollection* toDeleteCollection;
	HRESULT hr = CoCreateInstance(CLSID_PortableDevicePropVariantCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&toDeleteCollection));
	if (FAILED(hr) || toDeleteCollection == NULL)
	{
		return false;
	}

	//creating and adding our object
	PROPVARIANT pv;
	PropVariantInit(&pv);
	pv.vt = VT_LPWSTR;
	pv.pwszVal = new WCHAR[(_objectID.size() + 1) * sizeof(WCHAR)];
	if (pv.pwszVal == NULL)
	{
		return false;
	}
	wmemcpy(pv.pwszVal, _objectID.c_str(), (_objectID.size() + 1));
	toDeleteCollection->Add(&pv);

	hr = _usedConnection->getDeviceContentInterface()->Delete(PORTABLE_DEVICE_DELETE_NO_RECURSION, toDeleteCollection, NULL);

	delete pv.pwszVal;
	pv.pwszVal = NULL;
	PropVariantClear(&pv);
	_usedConnection->releaseDeviceContentInterface();

	return (SUCCEEDED(hr)) ? true : false;
}

bool MtpMappableDrive::MtpFsDirectory::deleteMe()
{
	if (!_evaluateChildren()) { return false; }
	//First delete recursively all stuff inside
	for (std::vector<MtpFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if(! (*iter)->deleteMe() )
		{
			return false;
		}
	}

	//now we should be empty -- let's check
	deleteSubCache();
	DirContentCounter cnter;
	bool succeeded = doForAllContent(cnter);
	if (!succeeded || cnter.getCount() != 0)
	{
		return false;
	}

	//now it is safe to delete our own node
	IPortableDevicePropVariantCollection* toDeleteCollection;
	HRESULT hr = CoCreateInstance(CLSID_PortableDevicePropVariantCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&toDeleteCollection));
	if (FAILED(hr) || toDeleteCollection == NULL)
	{
		return false;
	}

	//creating and adding our object
	PROPVARIANT pv;
	PropVariantInit(&pv);
	pv.vt = VT_LPWSTR;
	pv.pwszVal = new WCHAR[(_objectID.size() + 1) * sizeof(WCHAR)];
	if (pv.pwszVal == NULL)
	{
		return false;
	}
	wmemcpy(pv.pwszVal, _objectID.c_str(), (_objectID.size() + 1));
	toDeleteCollection->Add(&pv);

	hr = _usedConnection->getDeviceContentInterface()->Delete(PORTABLE_DEVICE_DELETE_NO_RECURSION, toDeleteCollection, NULL);

	delete pv.pwszVal;
	pv.pwszVal = NULL;
	PropVariantClear(&pv);
	_usedConnection->releaseDeviceContentInterface();

	return (SUCCEEDED(hr)) ? true : false;
}

bool MtpMappableDrive::MtpFsDirectory::createNewFolder(CaseInsensitiveWstring name)
{
	//get the properties for the new-to-create file
	IPortableDeviceValues* objectProperties;
	if (!_fillPropertiesForNewFolder(name.getActualCase(), &objectProperties) )
	{
		return false;
	}

	//Create new folder:
	PWSTR idOfNewObject;
	HRESULT hr = _usedConnection->getDeviceContentInterface()->CreateObjectWithPropertiesOnly(objectProperties, &idOfNewObject);
	if (FAILED(hr))
	{
		_usedConnection->releaseDeviceContentInterface();
		return false;
	}

	CoTaskMemFree(idOfNewObject);
	idOfNewObject = NULL;
	_usedConnection->releaseDeviceContentInterface();
	return true;
}

bool MtpMappableDrive::MtpFsDirectory::_fillPropertiesForNewFolder(std::wstring name, IPortableDeviceValues** values )
{
	//First, create the values object
	*values = nullptr;
	IPortableDeviceValues* tempValues;
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&tempValues));
	if (FAILED(hr))
	{
		return false;
	}

	//Get all the properties we need for folders
	//parent folder
	hr = tempValues->SetStringValue(WPD_OBJECT_PARENT_ID, _objectID.c_str());
	if (FAILED(hr))
	{
		return false;
	}
	//type
	hr = tempValues->SetGuidValue(WPD_OBJECT_CONTENT_TYPE, WPD_CONTENT_TYPE_FOLDER);
	if (FAILED(hr))
	{
		return false;
	}
	hr = tempValues->SetGuidValue(WPD_OBJECT_FORMAT, WPD_OBJECT_FORMAT_PROPERTIES_ONLY);
	if (FAILED(hr))
	{
		return false;
	}
	//folder name
	hr = tempValues->SetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, name.c_str());
	if (FAILED(hr))
	{
		return false;
	}
	//folder name, #2 because in Germany we say: "Doppelt haelt besser" xxD
	hr = tempValues->SetStringValue(WPD_OBJECT_NAME, name.c_str());
	if (FAILED(hr))
	{
		return false;
	}
	
	//got everything for now...
	*values = tempValues;
	return true;
}

bool MtpMappableDrive::MtpFsDirectory::doForAllContent(DoForAllDirContentFuctionObj& doWhat)
{
	if (!haveChildrenEvaluated)
	{
		if (!_evaluateChildren()) { return false; }
	}
	for (std::vector<MtpFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if (!doWhat.doForAllDirContentMethod(*iter))
		{
			return true;
		}
	}
	return true;
}

unsigned long long MtpFsNode::getUniqueId()
{
	std::string str = Utils::wstringToString(_objectID);
	unsigned long long ret = 0;
	for (std::string::iterator iter = str.begin(); iter != str.end(); iter++)
	{
		ret = ret | (*iter);
		ret = ret << 8;
	}
	return ret;
}

bool MtpFsNode::moveNode(AbstractFsNode* destDir)
{
	//This Method does not work! Reason: My device does not support it.
	MtpFsNode* destDirConc = dynamic_cast<MtpFsNode*>(destDir);
	if (destDirConc == NULL || !destDirConc->isDirectory()) { return false; }
	std::wstring moveDestId = destDirConc->_objectID;

	IPortableDevicePropVariantCollection* objToMoveColl;
	IPortableDevicePropVariantCollection* moveResults;
	HRESULT hr = CoCreateInstance(CLSID_PortableDevicePropVariantCollection, NULL, CLSCTX_INPROC_SERVER, IID_IPortableDevicePropVariantCollection, (VOID**)(&objToMoveColl));
	if (FAILED(hr)) { return false; }

	//creating and adding our object
	PROPVARIANT pv;
	PropVariantInit(&pv);
	pv.vt = VT_LPWSTR;
	pv.pwszVal = new WCHAR[(_objectID.size() + 1) * sizeof(WCHAR)];
	if (pv.pwszVal == NULL)
	{
		return false;
	}
	wmemcpy(pv.pwszVal, _objectID.c_str(), (_objectID.size() + 1));

	//moving...
	hr = _usedConnection->getDeviceContentInterface()->Move(objToMoveColl, moveDestId.c_str(), &moveResults);

	_parent->deleteSubCache();
	destDirConc->deleteSubCache();

	delete pv.pwszVal;
	pv.pwszVal = NULL;
	PropVariantClear(&pv);
	ActuallyDeleteComObject(objToMoveColl)
	if (FAILED(hr)) 
	{ 
		_usedConnection->releaseDeviceContentInterface();
		//moveResults->Release();
		return false; 
	}
	else 
	{ 
		_usedConnection->releaseDeviceContentInterface();
		ActuallyDeleteComObject(moveResults)
		return true;
	}
}

bool MtpFsNode::rename(std::wstring newName)
{
	IPortableDeviceProperties* props;
	IPortableDeviceValues* failPropColl;
	_usedConnection->getDeviceContentInterface()->Properties(&props);
	IPortableDeviceValues* writePropColl;
	HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&writePropColl));
	if (FAILED(hr) || writePropColl == NULL) { return false; }
	writePropColl->SetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, newName.c_str());

	hr = props->SetValues(this->_objectID.c_str(), writePropColl, &failPropColl);

	ActuallyDeleteComObject(props)
	if (FAILED(hr)) 
	{ 
		_usedConnection->releaseDeviceContentInterface();
		//failPropColl->Release();
		return false; 
	}
	else
	{
		_parent->deleteSubCache();
		ActuallyDeleteComObject(failPropColl)
		_usedConnection->releaseDeviceContentInterface();
		//don't need to look up write results, sample code doesn't do it either and values contained there are useless
		return true;
	}
}