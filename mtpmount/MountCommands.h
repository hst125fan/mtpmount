#pragma once
#include "SubCommandLibrary.h"
#include "..\dokanconnect\DokanDriveWrapper.h"
#include "..\mtpcache\FileCache.h"
#include "..\mtpcache\ConnectionSync.h"
#include "..\mtpaccess\ConnectionProvider.h"
#include <unordered_map>
#include <vector>

class DecideForGivenDriveName : public ConnectionSync::MapDriveDecider
{
private:
	std::wstring _match;
public:
	DecideForGivenDriveName(std::wstring match) : _match(match) { }
	virtual int decideDrive(std::vector<std::wstring>& availableDrives)
	{
		for (size_t i = 0; i < availableDrives.size(); i++)
		{
			if (availableDrives.at(i) == _match)
			{
				return i;
			}
		}
		return -1;
	}
};

class ActiveConnection
{
private:
	FileCache* _cache;
	FileCacheBackgroundJob* _backjob;
	ConnectionSync _syncer;
	typedef std::unordered_map<std::wstring, DokanDriveWrapper*> typeof_drivemap;
	typeof_drivemap _drivemap;
	std::ostream* _logger;
public:
	ActiveConnection(AbstractConnection* conn);
	bool mapDrive(std::wstring& name, char drvLetter);
	void unmapDrive(std::wstring& name);
	std::wstring driveLetterToName(char drvLetter);
	std::string getDriveLetters();
	void unmapAllDrives();
	bool isDriveMapped(const std::wstring& drive);
	void printMappings(std::ostream& stream, std::wstring devicename, std::string prefix = "" );
	void setLogOstream(std::ostream* stream);
	~ActiveConnection();
};


class MountAddressStore
{
public:
	class devStorageId
	{
	public:
		std::wstring connection;
		std::wstring storage;
		devStorageId(const std::wstring& conn, const std::wstring& store) : connection(conn), storage(store) { }
	};
	typedef size_t devStorageEnum;
private:
	static MountAddressStore* _instance;
	MountAddressStore() { }
	MountAddressStore(const MountAddressStore& store) = delete;
	const MountAddressStore& operator=(const MountAddressStore& store) = delete;
	typedef std::unordered_map<std::wstring, ActiveConnection*> typeof_connectionmap;
	typeof_connectionmap _connectionmap;
	typedef std::vector<devStorageId*> typeof_idvector;
	typedef std::map<devStorageId, size_t> typeof_idset;
	typeof_idvector _idvector;
	typeof_idset _idset;
public:
	static MountAddressStore& getInstance();
	ActiveConnection* openConnection(const std::wstring& devicename);
	void closeConnection(const std::wstring& devicename, std::ostream* log = NULL);
	bool isConnectionOpen(const std::wstring& devicename);
	void closeAllConnections();
	ActiveConnection* whoHasThisDriveLetter(char letter);
	std::wstring whoHasThisDriveLetterName(char letter);
	void printAllActive(std::ostream& stream);
	void printAllAvailable(std::ostream& stream);
	devStorageEnum toStorageEnum(const devStorageId& storageIdent);
	devStorageId toStorageId(devStorageEnum storageEnum);
	bool decideIdParameter(const std::string& parameter, std::wstring& devicename, std::wstring& storagename);
};

bool operator<(const MountAddressStore::devStorageId& a, const MountAddressStore::devStorageId& b);


class MountSubCommand : public AbstractSubCommand
{
public:
	MountSubCommand();
	virtual const char* getBriefDescription();
	virtual const char* getFullDescription();
	virtual int executeCommand(SubParameters& subparams, std::ostream& output);
};

class UnmountSubCommand : public AbstractSubCommand
{
public:
	UnmountSubCommand();
	virtual const char* getBriefDescription();
	virtual const char* getFullDescription();
	virtual int executeCommand(SubParameters& subparams, std::ostream& output);
};

class TerminateConnectionSubCommand : public AbstractSubCommand
{
public:
	TerminateConnectionSubCommand();
	virtual const char* getBriefDescription();
	virtual const char* getFullDescription();
	virtual int executeCommand(SubParameters& subparams, std::ostream& output);
};

class ListDrivesSubCommand : public AbstractSubCommand
{
public:
	ListDrivesSubCommand();
	virtual const char* getBriefDescription();
	virtual int executeCommand(SubParameters& subparams, std::ostream& output);
};



