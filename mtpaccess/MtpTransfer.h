#pragma once

#include <Windows.h>
#include <PortableDevice.h>
#include <PortableDeviceApi.h>

#include "Utils.h"
#include "AbstractFileSystem.h"
#include "AbstractConnection.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>

class MtpConnection;
class MtpMappableDrive;



class MtpConnection : public AbstractConnection
{
private:
	IPortableDevice* _dev;
	IPortableDeviceContent* _devcont;
	MtpConnection(IPortableDevice* dev, std::wstring& ident);
	~MtpConnection();
	//int _refcount;
	//std::wstring deletekey;
	friend class MtpConnectionProvider;
	std::vector<MtpMappableDrive*> _mappableDrives;
	bool _iKnowThatMyDeviceIsEmpty;
	bool _lookForMappableDrives();
public:
	//Obtain a list of all names of storage media found in the device
	//Parameters:
	//(1) OUT	std::vector<std::wstring>& putNamesHere		all the names
	//(2) IN	bool forceRefresh (OPTIONAL)				do a refresh on the connection to recognize changes
	//Returns: true on success, false on failure
	virtual bool getMappableDriveNames(std::vector<std::wstring>& putNamesHere, bool forceRefresh = false);

	//Obtain the count of storage media found in the device
	//Parameters:
	//(1) OUT	int& putCountHere				the count you asked for
	//(2) IN	bool forceRefresh (OPTIONAL)	do a refresh on the connection to recognize changes
	//Returns: true on success, false on failure
	virtual bool getMappableDriveCnt( int& putCountHere, bool forceRefresh = false );

	//search for storage media given its friendly name
	//Parameters:
	//(1) IN	std::wstring friendlyname	the name we are searching for
	//Returns: the mappable drive object with matching name on success, NULL on failure.
	virtual AbstractMappableDrive* getMDriveByFName(std::wstring friendlyname);

	IPortableDeviceContent* getDeviceContentInterface();
	void releaseDeviceContentInterface() { if (_devcont != NULL) { ActuallyDeleteComObject(_devcont) _devcont = NULL; } }

};

//This function object can be used to count all nodes within a directory
class DirContentCounter : public DoForAllDirContentFuctionObj
{
private:
	int count;
public:
	DirContentCounter() { count = 0; }
	int getCount() { return count; }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode) { count++; return true; }
};

class MtpFsNode : public AbstractFsNode
{
protected:
	std::wstring _objectID;
	MtpConnection* _usedConnection;
	MtpFsNode(std::wstring objectID, std::wstring friendlyname, MtpFsNode* parent, MtpConnection* usedConnection) : _objectID(objectID), _usedConnection(usedConnection), AbstractFsNode( friendlyname, parent ) { }
public:
	virtual void deleteSubCache() { }
	virtual bool deleteMe() = 0;
	virtual AbstractFsNode* getContentByName(CaseInsensitiveWstring subdirName) { return NULL; }
	virtual bool DEPRECATED getDirContent(std::vector<AbstractFsNode*>& dirContent, unsigned int from = 0, unsigned int until = UINT_MAX) { return false; }
	virtual bool doForAllContent(DoForAllDirContentFuctionObj& doWhat) { return false; }
	virtual bool contentToStream(std::ostream& stream) { return false; }
	virtual bool DEPRECATED StreamToContent(std::istream& stream) { return false; }
	virtual bool createNewFileWithStream(CaseInsensitiveWstring name, std::istream& stream, unsigned long long size, ULONGLONG modified, ULONGLONG created, ULONGLONG authored) { return false; }
	virtual bool createNewFolder(CaseInsensitiveWstring name) { return false; }
	virtual ~MtpFsNode() { }
	virtual unsigned long long getUniqueId();
	virtual bool moveNode(AbstractFsNode* destDir);
	virtual bool rename(std::wstring newName);
};

class MtpMappableDrive : public AbstractMappableDrive
{
private:
	MtpMappableDrive(MtpMappableDrive const& p) = delete;
	MtpMappableDrive& operator= (MtpMappableDrive const& p) = delete;

	class MtpFsDirectory : public MtpFsNode
	{
	private:
		std::vector<MtpFsNode*> _children;
		bool haveChildrenEvaluated;
		bool _evaluateChildren();
		bool _fillPropertiesForNewFile(std::wstring name, IPortableDeviceValues** values, unsigned long long filesize, FILETIME creationTime = { 0,0 }, FILETIME modificationTime = { 0,0 }, FILETIME authorizationTime = { 0,0 });
		bool _fillPropertiesForNewFolder(std::wstring name, IPortableDeviceValues** values );
	public:
		MtpFsDirectory(std::wstring objectID, std::wstring friendlyname, MtpFsNode* parent, MtpConnection* usedConnection) : haveChildrenEvaluated( false ), MtpFsNode( objectID, friendlyname, parent, usedConnection)  { }
		~MtpFsDirectory();
		virtual void deleteSubCache();
		virtual bool isDirectory() { return true; }
		virtual bool DEPRECATED getDirContent(std::vector<MtpFsNode*>& dirContent, unsigned int from = 0, unsigned int until = UINT_MAX);
		virtual bool doForAllContent(DoForAllDirContentFuctionObj& doWhat);
		virtual MtpFsNode* getContentByName(CaseInsensitiveWstring subdirName);
		virtual bool createNewFileWithStream(CaseInsensitiveWstring name, std::istream& stream, unsigned long long size, FILETIME modified, FILETIME created, FILETIME authored);
		virtual bool createNewFolder(CaseInsensitiveWstring name);
		virtual bool deleteMe();
	};
	class MtpFsFile : public MtpFsNode
	{
	private:
		unsigned long long payloadsize;
	public:
		MtpFsFile( std::wstring objectID, std::wstring friendlyname, MtpFsNode* parent, MtpConnection* usedConnection, unsigned long long size) : MtpFsNode(objectID, friendlyname, parent, usedConnection), payloadsize(size) { }
		virtual bool isDirectory() { return false; }
		virtual bool contentToStream(std::ostream& stream);
		virtual bool DEPRECATED StreamToContent(std::istream& stream) { return false; } //not supported by MTP
		virtual bool deleteMe();
		virtual unsigned long long getPayloadSize() { return payloadsize; }
		virtual void syncFileTime(FILETIME m, FILETIME c, FILETIME a);
	};
	MtpConnection* _usedConnection;
	friend class MtpConnection;
	std::wstring _objID;

	AbstractFsNode* _filePathToNode(FsPath& path);

	MtpMappableDrive(std::wstring& friendlyname, std::wstring& objID, MtpConnection* backref);
};


class MtpCommonObjectPropertiesException
{

};


class MtpCommonObjectProperties
{
private:
	PWSTR name;
	GUID contenttype;
	BOOL deleteallowance;
	FILETIME authored;
	FILETIME created;
	FILETIME modified;
	IPortableDeviceValues* values;
	IPortableDeviceProperties* props;
	IPortableDeviceKeyCollection* readprops;
public:
	MtpCommonObjectProperties(MtpConnection* conn, PWSTR objectid)
	{
		HRESULT res1 = conn->getDeviceContentInterface()->Properties(&props);
		if (FAILED(res1)) { throw MtpCommonObjectPropertiesException(); }
		res1 = CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&readprops));
		if (FAILED(res1)) { throw MtpCommonObjectPropertiesException(); }
		readprops->Add(WPD_OBJECT_ORIGINAL_FILE_NAME);
		readprops->Add(WPD_OBJECT_CONTENT_TYPE);
		readprops->Add(WPD_OBJECT_CAN_DELETE);
		readprops->Add(WPD_OBJECT_DATE_AUTHORED);
		readprops->Add(WPD_OBJECT_DATE_CREATED);
		readprops->Add(WPD_OBJECT_DATE_MODIFIED);
		res1 = props->GetValues(objectid, readprops, &values);
		if (FAILED(res1)) { conn->releaseDeviceContentInterface(); throw MtpCommonObjectPropertiesException(); }
		values->GetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, &name);
		values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE, &contenttype);
		values->GetBoolValue(WPD_OBJECT_CAN_DELETE, &deleteallowance);

		//only modification time is used!
		PROPVARIANT modificationTime;
		PropVariantInit(&modificationTime);
		SYSTEMTIME sysTimeIntermediate;
		values->GetValue(WPD_OBJECT_DATE_MODIFIED, &modificationTime);
		VariantTimeToSystemTime(modificationTime.date, &sysTimeIntermediate);
		SystemTimeToFileTime(&sysTimeIntermediate, &modified);
		PropVariantClear(&modificationTime);

		PROPVARIANT creationTime;
		PropVariantInit(&creationTime);
		SYSTEMTIME sysTimeIntermediate2;
		values->GetValue(WPD_OBJECT_DATE_CREATED, &creationTime);
		VariantTimeToSystemTime(creationTime.date, &sysTimeIntermediate2);
		SystemTimeToFileTime(&sysTimeIntermediate2, &created);
		PropVariantClear(&creationTime);

		PROPVARIANT authorizationTime;
		PropVariantInit(&authorizationTime);
		SYSTEMTIME sysTimeIntermediate3;
		values->GetValue(WPD_OBJECT_DATE_AUTHORED, &authorizationTime);
		VariantTimeToSystemTime(authorizationTime.date, &sysTimeIntermediate3);
		SystemTimeToFileTime(&sysTimeIntermediate3, &authored);
		PropVariantClear(&authorizationTime);
		conn->releaseDeviceContentInterface();
	}
	~MtpCommonObjectProperties()
	{
		ActuallyDeleteComObject(values)
		ActuallyDeleteComObject(props)
		ActuallyDeleteComObject(readprops)
	}
	BOOL getDeleteAllowance() { return deleteallowance; }
	PWSTR getName() { return name; }
	GUID getContentType() { return contenttype; }
	FILETIME getAuthorizationDate() { return authored; }
	FILETIME getCreationDate() { return created; }
	FILETIME getModificationDate() { return modified; }
};

class MtpCommonFileOnlyProperties
{
private:
	ULONGLONG size;
	IPortableDeviceValues* values;
	IPortableDeviceProperties* props;
	IPortableDeviceKeyCollection* readprops;
public:
	MtpCommonFileOnlyProperties(MtpConnection* conn, PWSTR objectid)
	{
		HRESULT res1 = conn->getDeviceContentInterface()->Properties(&props);
		if (FAILED(res1)) { conn->releaseDeviceContentInterface(); throw MtpCommonObjectPropertiesException(); }
		res1 = CoCreateInstance(CLSID_PortableDeviceKeyCollection, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&readprops));
		if (FAILED(res1)) { conn->releaseDeviceContentInterface(); throw MtpCommonObjectPropertiesException(); }
		readprops->Add(WPD_OBJECT_SIZE);
		res1 = props->GetValues(objectid, readprops, &values);
		if (FAILED(res1)) { conn->releaseDeviceContentInterface(); throw MtpCommonObjectPropertiesException(); }
		values->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE, &size);
		conn->releaseDeviceContentInterface();
	}
	~MtpCommonFileOnlyProperties()
	{
		ActuallyDeleteComObject(values)
		ActuallyDeleteComObject(props)
		ActuallyDeleteComObject(readprops)
	}
	ULONGLONG getSize() { return size; }
};