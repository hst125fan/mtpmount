#pragma once
#include "AbstractConnection.h"
#include "TestFileSystem.h"

class TestMappableDrive : public AbstractMappableDrive
{
private:
	TestMappableDrive(TestMappableDrive const& p) = delete;
	TestMappableDrive& operator= (TestMappableDrive const& p) = delete;
public:
	TestMappableDrive() : AbstractMappableDrive(L"TestDrive") { std::wstring n = L""; driveRoot = new TestFsDirectory(n, NULL); }
};

class TestConnection : public AbstractConnection
{
private:
	AbstractMappableDrive* myDrive;
public:
	TestConnection(std::wstring& ident) : AbstractConnection( ident) { myDrive = new TestMappableDrive; }
	~TestConnection() { delete myDrive; }

	virtual bool getMappableDriveCnt(int& putCountHere, bool forceRefresh = false) { putCountHere = 1; return true; }
	virtual AbstractMappableDrive* getMDriveByFName(std::wstring friendlyname) { return (friendlyname == L"TestDrive") ? myDrive : NULL; }
	virtual bool getMappableDriveNames(std::vector<std::wstring>& putNamesHere, bool forceRefresh = false) { putNamesHere.clear(); putNamesHere.emplace_back(L"TestDrive"); return true; }
};
