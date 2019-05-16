#include "ConnectionSync.h"
#include "..\mtpaccess\ConnectionProvider.h"
#include <iostream>
#include <thread>
#include "FileCache.h"

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

void threadRoutine1(ConnectionSync* s)
{
	WaitOnResult w1;
	WaitOnResult w2;
	ConnectionSync::FilePullRequest pull1(drvID, FsPath(L"testdir/test2.txt"), L"test2back.txt", &w1);
	ConnectionSync::FilePullRequest pull2(drvID, FsPath(L"test1.txt"), L"test1back.txt", &w2);
	s->enqueueFilePullRequest(pull1);
	s->enqueueFilePullRequest(pull2);
	if (!w1.wait()) { std::cout << "Failed 1/pull1" << std::endl; }
	if (!w2.wait()) { std::cout << "Failed 1/pull2" << std::endl; }
	std::cout << "Thread 1 out" << std::endl;
}

void threadRoutine2(ConnectionSync* s)
{
	WaitOnResult w1;
	WaitOnResult w2;
	WaitOnResult w3;
	WaitOnResult w4;
	ConnectionSync::TreeAccessRequest tree1(drvID, FsPath(L"testdir/test3.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest push1(drvID, FsPath(L"testdir/test3.txt"), L"test1.txt", &w1);
	ConnectionSync::TreeAccessRequest tree2(drvID, FsPath(L"testdir/x"), ConnectionSync::CREATE_AS_DIRECTORY, &w2);
	ConnectionSync::TreeAccessRequest tree3(drvID, FsPath(L"testdir/x/test4.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest push3(drvID, FsPath(L"testdir/x/test4.txt"), L"test2.txt", &w3);
	ConnectionSync::FilePushRequest push4(drvID, FsPath(L"test1.txt"), L"test2.txt", &w4);
	s->enqueueTreeAccessRequest(tree1);
	s->enqueueFilePushRequest(push1);
	s->enqueueTreeAccessRequest(tree2);
	s->enqueueTreeAccessRequest(tree3);
	s->enqueueFilePushRequest(push3);
	s->enqueueFilePushRequest(push4);
	if (!w1.wait()) { std::cout << "Failed 2/push1" << std::endl; }
	if (!w2.wait()) { std::cout << "Failed 2/tree2" << std::endl; }
	if (!w3.wait()) { std::cout << "Failed 2/push3" << std::endl; }
	if (!w4.wait()) { std::cout << "Failed 2/push4" << std::endl; }
	std::cout << "Thread 2 out" << std::endl;
}


int main()
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

	if (conn == NULL)
	{
		std::cout << "TestDevice not found!" << std::endl;
		return 1;
	}


	ConnectionSync syncer(conn);
	DecideForTestDrive d;
	drvID = syncer.mapNewDrive(d);
	if (drvID == -1)
	{
		std::cout << "TestDrvie not found!" << std::endl;
		return 1;
	}

	std::ofstream dbgtest1write("test1.txt");
	std::ofstream dbgtest2write("test2.txt");
	dbgtest1write << "TEST_TEST_TEST --- TEST1";
	dbgtest2write << "TEST_TEST_TEST --- TEST2";
	dbgtest1write.close();
	dbgtest2write.close();
	ConnectionSync::TreeAccessRequest req1(drvID, FsPath(L"testfile1"), ConnectionSync::CREATE_AS_FILE, NULL);
	bool r1 = syncer.doTreeAccessRequest(req1);
	ConnectionSync::FilePushRequest req2(drvID, FsPath(L"testfile1"), L"test1.txt", NULL);
	bool r2 = syncer.doFilePushRequest(req2);
	ConnectionSync::TreeAccessRequest req3(drvID, FsPath(L"testfile2"), ConnectionSync::CREATE_AS_FILE, NULL);
	bool r3 = syncer.doTreeAccessRequest(req3);

	if (!r1 || !r2 || !r3) { return 1; }

	std::cout << "PREP DONE" << std::endl;

	FileCache* myCache = new FileCache(syncer, L"cachefolder");

	std::wstring filename1 = myCache->beginAccessByFileName(FsPath(L"testfile1"), drvID, false);
	if (filename1 == L"ERROR") { std::cout << "didn't work" << std::endl; return 1; }
	std::ifstream file1(filename1);
	while (file1.good())
	{
		std::cout << (char)(file1.get());
	}
	file1.close();
	myCache->endFileAccessByName(FsPath(L"testfile1"), drvID, false);

	std::cout << "INTERMEDIATE POINT" << std::endl;

	std::wstring filename2 = myCache->beginAccessByFileName(FsPath(L"testfile2"), drvID, true);
	std::ofstream file2(filename2, std::ios::trunc);
	file2 << "TEST2TEST2TEST2TEST2TEST2TEST2";
	file2.close();
	myCache->endFileAccessByName(FsPath(L"testfile2"), drvID, true);

	delete myCache;
	std::cout << "\n\n";

	ConnectionSync::FilePullRequest lastreq(drvID, FsPath(L"testfile2"), L"testbackfile", NULL);
	syncer.doFilePullRequest(lastreq);


	return 0;
}