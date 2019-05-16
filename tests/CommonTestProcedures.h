#pragma once

//This header does not include actual tests, but procedures which are often needed in Tests.

#include "..\mtpaccess\AbstractConnection.h"
#include "..\mtpaccess\ConnectionProvider.h"
#include "..\mtpcache\ConnectionSync.h"
#include <map>
#include <string>

AbstractConnection* getTestConnection()
{
	AbstractConnection* conn = NULL;
	for (int i = 0; i < MtpConnectionProvider::getInstance().getDeviceCount(); i++)
	{
		std::wstring name;
		MtpConnectionProvider::getInstance().getFriendlyNameOfDevice(i, name);
		if (name == L"TestDevice")
		{
			MtpConnectionProvider::getInstance().openConnectionToDevice(i, &conn, false);
			break;
		}
	}
	return conn;
}

AbstractMappableDrive* getTestMapDrive()
{
	AbstractConnection* testConn = getTestConnection();
	if (testConn == NULL) { return NULL; }
	return testConn->getMDriveByFName(L"TestDrive");
}

class testCheckFolderContent : public DoForAllDirContentFuctionObj
{
private:
	std::map<std::wstring, bool> _whatIAmCheckingFor;
	bool haveMoreThanILookedFor;
public:
	testCheckFolderContent() : haveMoreThanILookedFor(false) { }
	void addCheckpoint(std::wstring name, bool isDir)
	{
		name.append(((isDir) ? L"D" : L"F"));
		_whatIAmCheckingFor.insert(std::pair<std::wstring, bool>(name, false));
	}
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
	{
		try
		{
			std::wstring nodename = currentNode->getNodeName();
			nodename.append(((currentNode->isDirectory()) ? L"D" : L"F"));
			_whatIAmCheckingFor.at(nodename) = true;
		}
		catch (std::out_of_range)
		{
			haveMoreThanILookedFor = true;
		}
		return true;
	}
	bool checkIfAllConditionsWereMet(bool allowMore = false)
	{
		if (!allowMore && haveMoreThanILookedFor) { return false; }
		for (std::map<std::wstring, bool>::iterator iter = _whatIAmCheckingFor.begin();
			iter != _whatIAmCheckingFor.end();
			iter++)
		{
			if (iter->second == false) { return false; }
		}
		return true;
	}
};

class DecideForTestDrive : public ConnectionSync::MapDriveDecider
{
public:
	virtual int decideDrive(std::vector<std::wstring>& availableDrives)
	{
		for (int i = 0; i < (int)(availableDrives.size()); i++)
		{
			if (availableDrives.at(i) == L"TestDrive")
			{
				return i;
			}
		}
		return -1;
	}
};


void getSyncronizedTestMapDrive(ConnectionSync& syncer)
{
	DecideForTestDrive d;
	syncer.mapNewDrive(d, true);
}

