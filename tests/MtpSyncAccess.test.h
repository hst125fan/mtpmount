#ifdef defTestActivateHeaderInclude

//define all headers  here
#include "CommonTestProcedures.h"
#include "..\mtpaccess\Utils.h"
#include "..\mtpcache\ConnectionSync.h"
#include <fstream>
#include "MtpSyncAccess.h"
#include "..\mtpaccess\MtpTransfer.h"

#endif

defNewTest(MtpSyncAccessTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	int drvID = -1;
	AbstractConnection* conn = getTestConnection();

	if (conn == NULL)
	{
		output( "TestDevice not found!");
		return false;
	}


	ConnectionSync syncer(conn);
	DecideForTestDrive d;
	drvID = syncer.mapNewDrive(d);
	if (drvID == -1)
	{
		output( "TestDrvie not found!" );
		return false;
	}

	WaitOnResult w1;
	WaitOnResult w2;
	WaitOnResult w3;
	ConnectionSync::TreeAccessRequest push1p(drvID, FsPath(L"test1.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest push1(drvID, FsPath(L"test1.mp3"), std::wstring(L"testdata/test_binary_file.mp3"), &w1);
	ConnectionSync::TreeAccessRequest push2(drvID, FsPath(L"testdir"), ConnectionSync::CREATE_AS_DIRECTORY, &w2);
	ConnectionSync::TreeAccessRequest push3p(drvID, FsPath(L"testdir/test2.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest push3(drvID, FsPath(L"testdir/test2.txt"), std::wstring(L"testdata/test_file_size"), &w3);
	syncer.enqueueTreeAccessRequest(push1p);
	syncer.enqueueFilePushRequest(push1);
	syncer.enqueueTreeAccessRequest(push2);
	syncer.enqueueTreeAccessRequest(push3p);
	syncer.enqueueFilePushRequest(push3);

	if (!w1.wait()) { output("prep1 failed"); }
	if (!w2.wait()) { output("prep2 failed"); }
	if (!w2.wait()) { output("prep3 failed"); }

	std::thread t1(threadRoutine1, std::pair<ConnectionSync*, int>(&syncer,drvID));
	std::thread t2(threadRoutine2, std::pair<ConnectionSync*, int>(&syncer, drvID));
	t1.join();
	t2.join();

	bool ok = (Utils::binaryCompareFiles(L"testdata/test_binary_file.mp3", L"testdata/test_binary_file_back.mp3") &&
		Utils::binaryCompareFiles(L"testdata/test_file_size", L"testdata/test_file_size_back"));

	if (!ok) { output("Retrieved files differ from sent files. This could be caused by a change in transfer prio"); }
	Utils::deleteOSfile(L"testdata/test_binary_file_back.mp3");
	Utils::deleteOSfile(L"testdata/test_file_size_back");
	return ok;
}

defNewTest(MtpSyncAccessImmediateDiscoveryTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	int drvID = -1;
	AbstractConnection* conn = getTestConnection();

	if (conn == NULL)
	{
		output("TestDevice not found!");
		return false;
	}


	ConnectionSync syncer(conn);
	DecideForTestDrive d;
	drvID = syncer.mapNewDrive(d);
	if (drvID == -1)
	{
		output("TestDrvie not found!");
		return false;
	}

	ConnectionSync::FilePushRequest prep2(drvID, FsPath(L"disturbfile.mp3"), L"testdata/test_binary_file_2.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep1(drvID, FsPath(L"disturbfile.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);

	syncer.doTreeAccessRequest(prep1);
	syncer.doFilePushRequest(prep2);

	WaitOnResult w1;
	WaitOnResult w2;
	WaitOnResult w3;
	CheckForTestFile check;
	ConnectionSync::TreeAccessRequest createFile(drvID, FsPath(L"testfile.mp3"), ConnectionSync::CREATE_AS_FILE, &w1);
	ConnectionSync::FilePushRequest writeFile(drvID, FsPath(L"testfile.mp3"), L"testdata/test_binary_file.mp3", &w2);
	ConnectionSync::TreeAccessRequest lookIfFileIsThere(drvID, FsPath(L"\\"), check, &w3);

	DirContentCounter disturbtest;
	ConnectionSync::TreeAccessRequest disturb1(drvID, FsPath(L"\\"), disturbtest, NULL);
	ConnectionSync::FilePullRequest disturb2(drvID, FsPath(L"disturbfile.mp3"), L"testdata/disturb.mp3", NULL);
	ConnectionSync::TreeAccessRequest disturb3(drvID, FsPath(L"disturbdir"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);

	syncer.enqueueTreeAccessRequest(disturb1);
	syncer.enqueueFilePullRequest(disturb2);
	syncer.enqueueTreeAccessRequest(disturb3);
	syncer.enqueueTreeAccessRequest(createFile);
	syncer.enqueueFilePushRequest(writeFile);
	syncer.enqueueTreeAccessRequest(lookIfFileIsThere);

	bool result = true;
	if (!w1.wait())
	{
		output("File creation on device failed");
		result = false;
	}

	if (!w2.wait())
	{
		output("File write on device failed");
		result = false;
	}

	if (!w3.wait())
	{
		output("Search for file on device failed");
		result = false;
	}

	if (!check.isFound())
	{
		output("File not found on device");
		result = false;
	}

	Utils::deleteOSfile(L"testdata/disturb.mp3");
	return result;
}