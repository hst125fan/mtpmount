#ifdef defTestActivateHeaderInclude

#include "CommonTestProcedures.h"
#include "../mtpaccess/Utils.h"
#pragma warning (disable : 4005)
#include "../dokanconnect/DokanDriveWrapper.h"
#pragma warning (default : 4005)
#include "../mtpaccess/ConnectionProvider.h"
#include <Windows.h>
#endif


defNewTest(DokanAccessTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	//obtain connection
	AbstractConnection* conn = getTestConnection();

	//syncronize connection
	ConnectionSync* sync = new ConnectionSync(conn);

	//map device drive (in syncronized connection)
	DecideForTestDrive d;
	int drvID = sync->mapNewDrive(d, false);

	//push some content so the device is not empty
	ConnectionSync::TreeAccessRequest prep1(drvID, FsPath(L"testfile.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep2(drvID, FsPath(L"testfile.txt"), L"testdata/test_file_size", NULL);
	ConnectionSync::TreeAccessRequest prep3(drvID, FsPath(L"testfile.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep4(drvID, FsPath(L"testfile.mp3"), L"testdata/test_binary_file.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep5(drvID, FsPath(L"testdir"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	ConnectionSync::TreeAccessRequest prep6(drvID, FsPath(L"testdir/test2.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep7(drvID, FsPath(L"testdir/test2.mp3"), L"testdata/test_binary_file_2.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep8(drvID, FsPath(L"deleteme.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep9(drvID, FsPath(L"deleteme.txt"), L"testdata/test_file_size", NULL);
	ConnectionSync::TreeAccessRequest prep10(drvID, FsPath(L"deletedir"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	ConnectionSync::TreeAccessRequest prep11(drvID, FsPath(L"deletedir/unempty.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep12(drvID, FsPath(L"deletedir/unempty.txt"), L"testdata/test_file_size", NULL);
	ConnectionSync::TreeAccessRequest prep13(drvID, FsPath(L"renameme.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep14(drvID, FsPath(L"renameme.mp3"), L"testdata/test_binary_file_2.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep15(drvID, FsPath(L"move_here"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	ConnectionSync::TreeAccessRequest prep16(drvID, FsPath(L"to_rename"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	ConnectionSync::TreeAccessRequest prep17(drvID, FsPath(L"to_rename/cont.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep18(drvID, FsPath(L"to_rename/cont.txt"), L"testdata/test_file_size", NULL);
	bool p1ok = sync->doTreeAccessRequest(prep1);
	bool p2ok = sync->doFilePushRequest(prep2);
	bool p3ok = sync->doTreeAccessRequest(prep3);
	bool p4ok = sync->doFilePushRequest(prep4);
	bool p5ok = sync->doTreeAccessRequest(prep5);
	bool p6ok = sync->doTreeAccessRequest(prep6);
	bool p7ok = sync->doFilePushRequest(prep7);
	bool p8ok = sync->doTreeAccessRequest(prep8);
	bool p9ok = sync->doFilePushRequest(prep9);
	bool p10ok = sync->doTreeAccessRequest(prep10);
	bool p11ok = sync->doTreeAccessRequest(prep11);
	bool p12ok = sync->doFilePushRequest(prep12);
	bool p13ok = sync->doTreeAccessRequest(prep13);
	bool p14ok = sync->doFilePushRequest(prep14);
	bool p15ok = sync->doTreeAccessRequest(prep15);
	bool p16ok = sync->doTreeAccessRequest(prep16);
	bool p17ok = sync->doTreeAccessRequest(prep17);
	bool p18ok = sync->doFilePushRequest(prep18);
	if (!p1ok || !p2ok || !p3ok || !p4ok || !p5ok || !p6ok || !p7ok || !p8ok || !p9ok || !p10ok || !p11ok || !p12ok || !p13ok || !p14ok || !p15ok || !p16ok || !p17ok || !p18ok)
	{
		output("Prep failed: could not write initial content to device");
		return false;
	}

	//activate cache
	FileCache* myCache = new FileCache(*sync, L"testdata\\cache");

	//find out which drive letter to use
	std::string drvletters = Utils::getAvailableDrives();
	char useThisDriveLetter = drvletters[0];
	std::string tdrvlet;
	std::wstring tdrvletw;
	tdrvlet.append(1, useThisDriveLetter);
	tdrvletw.append(1, useThisDriveLetter);

	//activate virtual windows drive (using Dokan)
	DokanDriveWrapper* wrap = new DokanDriveWrapper(*sync, *myCache, drvID, useThisDriveLetter, L"TESTVOLUME");

	std::string startinfo("Executing test cases on virtual drive " + tdrvlet);
	output(startinfo);

	//=============================== END PREP - BEGIN TEST ===============================

	//	<Initial drive content>
	//	|-[deletedir]
	//		|-(unempty.txt)		test_file_size
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file_2.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(deleteme.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(testfile.mp3)		test_binary_file.mp3
	//	|-(testfile.txt)		test_file_size
	
	//Test case 1: Copy content from device to Windows - evaluate directly
	output("Executing Test case 1...");
	BOOL tc1_copyresult = CopyFile(std::string(tdrvlet + ":\\testfile.mp3").c_str(), "testdata\\test_binary_file_back.mp3", TRUE);
	bool tc1_compareresult = Utils::binaryCompareFiles(L"testdata/test_binary_file.mp3", L"testdata/test_binary_file_back.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_back.mp3");
	if (!tc1_copyresult || !tc1_compareresult)
	{
		output("Test case 1 failed: Copy content from device to windows");
		return false;
	}

	//	<Drive content after Test case 1 (unchanged)>
	//	|-[deletedir]
	//		|-(unempty.txt)		test_file_size
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file_2.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(deleteme.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(testfile.mp3)		test_binary_file.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 2: Read-compare a file directly on device- evaluate directly
	output("Executing Test case 2...");
	bool tc2_compareresult1 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", std::wstring(tdrvletw + L":\\testdir\\test2.mp3").c_str());
	bool tc2_compareresult2 = Utils::binaryCompareFiles(L"testdata/test_file_size", std::wstring(tdrvletw + L":\\testfile.txt").c_str());
	if (!tc2_compareresult1 || !tc2_compareresult2)
	{
		output("Test case 2 failed: Read-compare file on device");
		return false;
	}

	//	<Drive content after Test case 2 (unchanged)>
	//	|-[deletedir]
	//		|-(unempty.txt)		test_file_size
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file_2.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(deleteme.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(testfile.mp3)		test_binary_file.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 3: Copy a file onto the device which was not there before - needs delayed evaluation
	output("Executing Test case 3...");
	BOOL tc3_copyresult = CopyFile("testdata\\test_binary_file.mp3",std::string(tdrvlet + ":\\test3.mp3").c_str(), TRUE);
	bool tc3_compareresult = Utils::binaryCompareFiles(L"testdata/test_binary_file.mp3", std::wstring(tdrvletw + L":\\test3.mp3"));
	if (!tc3_copyresult || !tc3_compareresult)
	{
		output("Test case 3 failed: Copy new content to device");
		return false;
	}

	//	<Drive content after Test case 3 (changed)>
	//	|-[deletedir]
	//		|-(unempty.txt)		test_file_size
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file_2.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(deleteme.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 4: Overwrite content on the device - needs delayed evaluation
	output("Executing Test case 4...");
	BOOL tc4_copyresult = CopyFile("testdata\\test_binary_file.mp3", std::string(tdrvlet + ":\\testdir\\test2.mp3").c_str(), FALSE);
	BOOL tc4_copyresult2 = CopyFile("testdata\\test_binary_file_2.mp3", std::string(tdrvlet + ":\\testfile.mp3").c_str(), FALSE);
	bool tc4_compareresult = Utils::binaryCompareFiles(L"testdata/test_binary_file.mp3", std::wstring(tdrvletw + L":\\testdir\\test2.mp3"));
	bool tc4_compareresult2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", std::wstring(tdrvletw + L":\\testfile.mp3"));
	if (!tc4_copyresult || !tc4_compareresult || !tc4_copyresult2 || !tc4_compareresult2)
	{
		output("Test case 4 failed: Overwrite content on the device");
		return false;
	}

	//	<Drive content after Test case 4 (changed)>
	//	|-[deletedir]
	//		|-(unempty.txt)		test_file_size
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(deleteme.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 5: Delete file on device - needs delayed evaluation (which is file tree check)
	output("Executing Test case 5...");
	BOOL tc5_deleteresult = DeleteFileW(std::wstring(tdrvletw + L":\\deleteme.txt").c_str());
	DWORD tc5_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\deleteme.txt").c_str());
	DWORD tc5_error = GetLastError();
	if (!tc5_deleteresult || tc5_checkAttribs != INVALID_FILE_ATTRIBUTES || tc5_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 5 failed: Delete File on Device");
		return false;
	}

	//	<Drive content after Test case 5 (changed)>
	//	|-[deletedir]
	//		|-(unempty.txt)		test_file_size
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 6: Delete directory on device - needs delayed evaluation (which is file tree check)
	output("Executing Test case 6...");
	std::string tc6_cmd = "rmdir /s /q ";
	tc6_cmd.append(tdrvlet);
	tc6_cmd.append(":\\deletedir");
	int tc6_deleteresult = system(tc6_cmd.c_str());
	DWORD tc6_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\deletedir").c_str());
	DWORD tc6_error = GetLastError();
	if (tc6_deleteresult != 0 || tc6_checkAttribs != INVALID_FILE_ATTRIBUTES || tc6_error != ERROR_FILE_NOT_FOUND)
	{
		std::cout << tc6_deleteresult << "/" << tc6_checkAttribs << "/" << tc6_error << std::endl;
		output("Test case 6 failed: Delete Directory on Device");
		return false;
	}

	//	<Drive content after Test case 6 (changed)>
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(renameme.mp3)		test_binary_file_2.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 7A: Rename file on device - needs delayed evaluation
	output("Executing Test case 7A...");
	BOOL tc7_moveresult = MoveFile(std::string(tdrvlet + ":\\renameme.mp3").c_str(), std::string(tdrvlet + ":\\renamed.mp3").c_str());
	bool tc7_compareresult = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", std::wstring(tdrvletw + L":\\renamed.mp3"));
	DWORD tc7_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\renameme.mp3").c_str());
	DWORD tc7_error = GetLastError();

	if (!tc7_moveresult || !tc7_compareresult || tc7_checkAttribs != INVALID_FILE_ATTRIBUTES || tc7_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 7A failed: Rename File on Device");
		return false;
	}

	//	<Drive content after Test case 7A (changed)>
	//	|-[move_here]
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(renamed.mp3)			test_binary_file_2.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 7B: Move file on device - needs delayed evaluation
	output("Executing Test case 7B...");
	BOOL tc7b_moveresult = MoveFile(std::string(tdrvlet + ":\\renamed.mp3").c_str(), std::string(tdrvlet + ":\\testdir\\renamed.mp3").c_str());
	bool tc7b_compareresult = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", std::wstring(tdrvletw + L":\\testdir\\renamed.mp3"));
	DWORD tc7b_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\renamed.mp3").c_str());
	DWORD tc7b_error = GetLastError();

	if (!tc7b_moveresult || !tc7b_compareresult || tc7b_checkAttribs != INVALID_FILE_ATTRIBUTES || tc7b_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 7B failed: Move File on Device");
		return false;
	}

	//	<Drive content after Test case 7B (changed)>
	//	|-[move_here]
	//	|-[testdir]
	//		|-(renamed.mp3)		test_binary_file_2.mp3
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 7C: Move and Rename file on device - needs delayed evaluation
	output("Executing Test case 7C...");
	BOOL tc7c_moveresult = MoveFile(std::string(tdrvlet + ":\\testdir\\renamed.mp3").c_str(), std::string(tdrvlet + ":\\move_here\\moved.mp3").c_str());
	bool tc7c_compareresult = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", std::wstring(tdrvletw + L":\\move_here\\moved.mp3"));
	DWORD tc7c_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\testdir\\renamed.mp3").c_str());
	DWORD tc7c_error = GetLastError();

	if (!tc7c_moveresult || !tc7c_compareresult || tc7c_checkAttribs != INVALID_FILE_ATTRIBUTES || tc7c_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 7C failed: Move File on Device");
		return false;
	}

	//	<Drive content after Test case 7C (changed)>
	//	|-[move_here]
	//		|-(moved.mp3)		test_binary_file_2.mp3
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_rename]
	//		|-(cont.txt)		test_file_size
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 8A: Move and Rename directory on device - needs delayed evaluation
	output("Executing Test case 8A...");
	BOOL tc8_moveresult = MoveFile(std::string(tdrvlet + ":\\to_rename").c_str(), std::string(tdrvlet + ":\\to_move").c_str());
	bool tc8_compareresult = Utils::binaryCompareFiles(L"testdata/test_file_size", std::wstring(tdrvletw + L":\\to_move\\cont.txt"));
	DWORD tc8_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\to_rename").c_str());
	DWORD tc8_error = GetLastError();

	if (!tc8_moveresult || !tc8_compareresult || tc8_checkAttribs != INVALID_FILE_ATTRIBUTES || tc8_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 8A failed: Rename Directory on Device");
		return false;
	}

	//	<Drive content after Test case 8A (changed)>
	//	|-[move_here]
	//		|-(moved.mp3)		test_binary_file_2.mp3
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-[to_move]
	//		|-(cont.txt)		test_file_size
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 8B: Move directory on device - needs delayed evaluation
	output("Executing Test case 8B...");
	BOOL tc8b_moveresult = MoveFile(std::string(tdrvlet + ":\\to_move").c_str(), std::string(tdrvlet + ":\\testdir\\to_move").c_str());
	bool tc8b_compareresult = Utils::binaryCompareFiles(L"testdata/test_file_size", std::wstring(tdrvletw + L":\\testdir\\to_move\\cont.txt"));
	DWORD tc8b_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\to_move").c_str());
	DWORD tc8b_error = GetLastError();

	if (!tc8b_moveresult || !tc8b_compareresult || tc8b_checkAttribs != INVALID_FILE_ATTRIBUTES || tc8b_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 8B failed: Move File on Device");
		return false;
	}

	//	<Drive content after Test case 8B (changed)>
	//	|-[move_here]
	//		|-(moved.mp3)		test_binary_file_2.mp3
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//		|-[to_move]
	//			|-(cont.txt)		test_file_size
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 8C: Move and Rename file on device - needs delayed evaluation
	output("Executing Test case 8C...");
	BOOL tc8c_moveresult = MoveFile(std::string(tdrvlet + ":\\testdir\\to_move").c_str(), std::string(tdrvlet + ":\\move_here\\moved_aswell").c_str());
	bool tc8c_compareresult = Utils::binaryCompareFiles(L"testdata/test_file_size", std::wstring(tdrvletw + L":\\move_here\\moved_aswell\\cont.txt"));
	DWORD tc8c_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\testdir\\to_move").c_str());
	DWORD tc8c_error = GetLastError();

	if (!tc8c_moveresult || !tc8c_compareresult || tc8c_checkAttribs != INVALID_FILE_ATTRIBUTES || tc8c_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 8C failed: Move File on Device");
		return false;
	}

	//	<Drive content after Test case 8C (changed)>
	//	|-[move_here]
	//		|-(moved.mp3)		test_binary_file_2.mp3
	//		|-[moved_aswell]
	//			|-(cont.txt)		test_file_size
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 9A: Copy file which is already on device
	output("Executing Test case 9A...");
	BOOL tc9_copyresult = CopyFile(std::string(tdrvlet + ":\\testfile.mp3").c_str(), std::string(tdrvlet + ":\\testfile_copied.mp3").c_str(), TRUE);
	bool tc9_compareresult1 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", std::wstring(tdrvletw + L":\\testfile_copied.mp3"));
	bool tc9_compareresult2 = Utils::binaryCompareFiles(std::wstring(tdrvletw + L":\\testfile.mp3"), std::wstring(tdrvletw + L":\\testfile_copied.mp3"));

	if (!tc9_copyresult || !tc9_compareresult1 || !tc9_compareresult2)
	{
		output("Test case 9A failed: Copy File on Device");
		return false;
	}

	//	<Drive content after Test case 9A (changed)>
	//	|-[move_here]
	//		|-(moved.mp3)		test_binary_file_2.mp3
	//		|-[moved_aswell]
	//			|-(cont.txt)		test_file_size
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile_copied.mp3)	test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//Test case 9B: Copy directory which is already on device
	output("Executing Test case 9B...");
	std::string tc9b_cmd = "xcopy /e /i ";
	tc9b_cmd.append(tdrvlet);
	tc9b_cmd.append(":\\move_here\\moved_aswell ");
	tc9b_cmd.append(tdrvlet);
	tc9b_cmd.append(":\\testdir\\dircpy");
	int tc9b_copyresult = system(tc9b_cmd.c_str());
	bool tc9b_compareresult1 = Utils::binaryCompareFiles(L"testdata/test_file_size", std::wstring(tdrvletw + L":\\testdir\\dircpy\\cont.txt"));
	bool tc9b_compareresult2 = Utils::binaryCompareFiles(std::wstring(tdrvletw + L":\\move_here\\moved_aswell\\cont.txt"), std::wstring(tdrvletw + L":\\testdir\\dircpy\\cont.txt"));

	if (tc9b_copyresult != 0 || !tc9b_compareresult1 || !tc9b_compareresult2)
	{
		output("Test case 9B failed: Copy Directory on Device");
		return false;
	}

	//	<Drive content after Test case 9B (changed)>
	//	|-[move_here]
	//		|-(moved.mp3)		test_binary_file_2.mp3
	//		|-[moved_aswell]
	//			|-(cont.txt)		test_file_size
	//	|-[testdir]
	//		|-(test2.mp3)		test_binary_file.mp3
	//		|-[dircpy]
	//			|-(cont.txt)		test_file_size
	//	|-(test3.mp3)			test_binary_file.mp3
	//	|-(testfile.mp3)		test_binary_file_2.mp3
	//	|-(testfile_copied.mp3)	test_binary_file_2.mp3
	//	|-(testfile.txt)		test_file_size

	//files should now be inactive
	bool inactivity1 = myCache->isClearOfReadWrite(FsPath(L"move_here/moved.mp3"), drvID);
	bool inactivity2 = myCache->isClearOfReadWrite(FsPath(L"testdir/dircpy/cont.txt"), drvID);
	bool inactivity3 = myCache->isClearOfReadWrite(FsPath(L"testfile_copied.mp3"), drvID);
	bool inactivity4 = myCache->isClearOfReadWrite(FsPath(L"testfile.mp3"), drvID);
	bool inactivity5 = myCache->isClearOfReadWrite(FsPath(L"move_here/moved_aswell/cont.txt"), drvID);
	bool inactivity6 = myCache->isClearOfReadWrite(FsPath(L"testfile.mp3"), drvID);
	bool inactivity7 = myCache->isClearOfReadWrite(FsPath(L"test3.mp3"), drvID);
	bool inactivity8 = myCache->isClearOfReadWrite(FsPath(L"testdir/test2.mp3"), drvID);

	if (!inactivity1 || !inactivity2 || !inactivity3 || !inactivity4 || !inactivity5 || !inactivity6 || !inactivity7 || !inactivity8)
	{
		output("Some cache files were not inactive after termination of test cases");
		return false;
	}

	//=============================== END TEST - BEGIN TIDY UP ===============================

	output("Test cases completed");

	delete wrap;
	output("Drive unmapped");
	delete myCache;


	//File Tree Check
	output("File Tree Check");
	testCheckFolderContent rootchecker;
	rootchecker.addCheckpoint(L"testdir", true);
	rootchecker.addCheckpoint(L"move_here", true);
	rootchecker.addCheckpoint(L"test3.mp3", false);
	rootchecker.addCheckpoint(L"testfile.mp3", false);
	rootchecker.addCheckpoint(L"testfile_copied.mp3", false);
	rootchecker.addCheckpoint(L"testfile.txt", false);
	ConnectionSync::TreeAccessRequest treeCheckRoot(drvID, FsPath(L"\\"), rootchecker, NULL);
	bool rootCheckRes = sync->doTreeAccessRequest(treeCheckRoot);
	if (!rootchecker.checkIfAllConditionsWereMet(false) || !rootCheckRes)
	{
		output("File Tree check failed (root level): Expected conditions were not met");
		return false;
	}

	testCheckFolderContent testdirchecker;
	testdirchecker.addCheckpoint(L"test2.mp3", false);
	testdirchecker.addCheckpoint(L"dircpy", true);
	ConnectionSync::TreeAccessRequest treeCheckTestdir(drvID, FsPath(L"testdir"), testdirchecker, NULL);
	bool testdirCheckRes = sync->doTreeAccessRequest(treeCheckTestdir);
	if (!testdirchecker.checkIfAllConditionsWereMet(false) || !testdirCheckRes)
	{
		output("File Tree check failed (folder testdir): Expected conditions were not met");
		return false;
	}

	testCheckFolderContent moveheredirchecker;
	moveheredirchecker.addCheckpoint(L"moved.mp3", false);
	moveheredirchecker.addCheckpoint(L"moved_aswell", true);
	ConnectionSync::TreeAccessRequest treeCheckMoveHere(drvID, FsPath(L"move_here"), moveheredirchecker, NULL);
	bool movehereCheckRes = sync->doTreeAccessRequest(treeCheckMoveHere);
	if (!moveheredirchecker.checkIfAllConditionsWereMet(false) || !movehereCheckRes)
	{
		output("File Tree check failed (folder move_here): Expected conditions were not met");
		return false;
	}

	testCheckFolderContent movedfolderchecker;
	movedfolderchecker.addCheckpoint(L"cont.txt", false);
	ConnectionSync::TreeAccessRequest treeCheckMoved(drvID, FsPath(L"move_here/moved_aswell"), movedfolderchecker, NULL);
	bool movedfCheckRes = sync->doTreeAccessRequest(treeCheckMoved);
	if (!movedfolderchecker.checkIfAllConditionsWereMet(false) || !movedfCheckRes)
	{
		output("File Tree check failed (folder moved_aswell): Expected conditions were not met");
		return false;
	}

	testCheckFolderContent copiedfolderchecker;
	copiedfolderchecker.addCheckpoint(L"cont.txt", false);
	ConnectionSync::TreeAccessRequest treeCheckCopied(drvID, FsPath(L"testdir/dircpy"), copiedfolderchecker, NULL);
	bool copiedfCheckRes = sync->doTreeAccessRequest(treeCheckCopied);
	if (!copiedfolderchecker.checkIfAllConditionsWereMet(false) || !copiedfCheckRes)
	{
		output("File Tree check failed (folder dircpy): Expected conditions were not met");
		return false;
	}

	//Test 3 delayed eval
	output("Evaluation test case 3");
	ConnectionSync::FilePullRequest checkpull1(drvID, FsPath(L"test3.mp3"), L"testdata/test_binary_file_back.mp3", NULL);
	bool cp1res1 = sync->doFilePullRequest(checkpull1);
	bool cp1res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_back.mp3", L"testdata/test_binary_file.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_back.mp3");
	if (!cp1res1 || !cp1res2)
	{
		output("Test case 3 failed: Copy new content to device");
		return false;
	}

	//Test 4 delayed eval
	output("Evaluation test case 4");
	ConnectionSync::FilePullRequest checkpull2(drvID, FsPath(L"testdir/test2.mp3"), L"testdata/test_binary_file_back.mp3", NULL);
	ConnectionSync::FilePullRequest checkpull3(drvID, FsPath(L"testfile.mp3"), L"testdata/test_binary_file_2_back.mp3", NULL);
	bool cp2res1 = sync->doFilePullRequest(checkpull2);
	bool cp2res3 = sync->doFilePullRequest(checkpull3);
	bool cp2res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_back.mp3", L"testdata/test_binary_file.mp3");
	bool cp2res4 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2_back.mp3", L"testdata/test_binary_file_2.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_back.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_2_back.mp3");
	if (!cp2res1 || !cp2res2 || !cp2res3 || !cp2res4)
	{
		output("Test case 4 failed: Overwrite content on the device");
		return false;
	}

	//Test 7 delayed eval
	output("Evaluation test case 7");
	ConnectionSync::FilePullRequest checkpull4(drvID, FsPath(L"move_here/moved.mp3"), L"testdata/test_binary_file_2_back.mp3", NULL);
	bool cp3res1 = sync->doFilePullRequest(checkpull4);
	bool cp3res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2_back.mp3", L"testdata/test_binary_file_2.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_2_back.mp3");
	if (!cp3res1 || !cp3res2)
	{
		output("Test case 7 failed: Rename/Move files");
		return false;
	}

	//Test 8 delayed eval
	output("Evaluation test case 8");
	ConnectionSync::FilePullRequest checkpull5(drvID, FsPath(L"move_here/moved_aswell/cont.txt"), L"testdata/test_file_size_back", NULL);
	bool cp4res1 = sync->doFilePullRequest(checkpull5);
	bool cp4res2 = Utils::binaryCompareFiles(L"testdata/test_file_size_back", L"testdata/test_file_size");
	Utils::deleteOSfile(L"testdata/test_file_size_back");
	if (!cp4res1 || !cp4res2)
	{
		output("Test case 8 failed: Rename/Move directories");
		return false;
	}

	//Test 9 delayed eval
	output("Evaluation test case 9");
	ConnectionSync::FilePullRequest checkpull6(drvID, FsPath(L"testdir/dircpy/cont.txt"), L"testdata/test_file_size_back", NULL);
	ConnectionSync::FilePullRequest checkpull7(drvID, FsPath(L"testfile_copied.mp3"), L"testdata/test_binary_file_2_back.mp3", NULL);
	bool cp5res1 = sync->doFilePullRequest(checkpull7);
	bool cp5res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2_back.mp3", L"testdata/test_binary_file_2.mp3");
	bool cp5res3 = sync->doFilePullRequest(checkpull6);
	bool cp5res4 = Utils::binaryCompareFiles(L"testdata/test_file_size_back", L"testdata/test_file_size");
	Utils::deleteOSfile(L"testdata/test_binary_file_2_back.mp3");
	Utils::deleteOSfile(L"testdata/test_file_size_back");
	if (!cp5res1 || !cp5res2 || !cp5res3 || !cp5res4)
	{
		output("Test case 9 failed: Copy files/directories");
		return false;
	}

	output("logoff");
	delete sync;

	output("SUCCESSFUL");
	//evaluation result
	return true;
}