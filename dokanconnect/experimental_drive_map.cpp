#include "MemLeakDetection.h"

#include "DokanDriveWrapper.h"
#include "..\mtpaccess\ConnectionProvider.h"
#include <iostream>


int drvID = -1;

class DecideForTestDrive : public ConnectionSync::MapDriveDecider
{
public:
	virtual int decideDrive(std::vector<std::wstring>& availableDrives)
	{
		for (size_t i = 0; i < availableDrives.size(); i++)
		{
			if (availableDrives.at(i) == L"TestDrive")
			{
				return i;
			}
		}
		return -1;
	}
};

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	AutoDeletePointer<MtpConnectionProvider> adp_mtpcp(&MtpConnectionProvider::getInstance());

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

	if (conn == NULL)
	{
		std::cout << "TestDevice not found!" << std::endl;
		return 1;
	}


	ConnectionSync* syncer = new ConnectionSync(conn);
	DecideForTestDrive d;
	drvID = syncer->mapNewDrive(d);
	if (drvID == -1)
	{
		std::cout << "TestDrvie not found!" << std::endl;
		return 1;
	}

	ConnectionSync::TreeAccessRequest prep1(drvID, FsPath(L"testfile.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep2(drvID, FsPath(L"testfile.txt"), L"testfile.txt", NULL);
	ConnectionSync::TreeAccessRequest prep3(drvID, FsPath(L"testfile.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep4(drvID, FsPath(L"testfile.mp3"), L"testfile.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep5(drvID, FsPath(L"testdir"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	syncer->doTreeAccessRequest(prep1);
	if (!syncer->doFilePushRequest(prep2)) { return 1; }
	syncer->doTreeAccessRequest(prep3);
	if (!syncer->doFilePushRequest(prep4)) { return 1; }
	syncer->doTreeAccessRequest(prep5);

	FileCache* myCache = new FileCache(*syncer, L"cachefolder");

	DokanDriveWrapper* wrap = new DokanDriveWrapper(*syncer, *myCache, drvID, L'N', L"TESTVOLUME");
	system("pause");
	std::cout << "Hands off!" << std::endl;
	Sleep(5000);
	delete wrap;
	delete myCache;

	ConnectionSync::FilePullRequest after1(drvID, FsPath(L"testfile.txt"), L"testfile.txt", NULL);
	if (!syncer->doFilePullRequest(after1))
	{
		std::cout << "sync error" << std::endl;
	}

	delete syncer;
	system("pause");
}