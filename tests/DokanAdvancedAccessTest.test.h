//This header is not active yet

#ifdef defTestActivateHeaderInclude
#include "CommonTestProcedures.h"
#include "../mtpaccess/Utils.h"
#pragma warning (disable : 4005)
#include "../dokanconnect/DokanDriveWrapper.h"
#pragma warning (default : 4005)
#include "../mtpaccess/ConnectionProvider.h"
#include <Windows.h>
#endif

defNewTest(DokanAdvancedAccessTest)
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

	ConnectionSync::TreeAccessRequest prep1(drvID, FsPath(L"movefile.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep2(drvID, FsPath(L"movefile.mp3"), L"testdata/test_binary_file.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep3(drvID, FsPath(L"move_here"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	ConnectionSync::TreeAccessRequest prep4(drvID, FsPath(L"movedir"), ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	ConnectionSync::TreeAccessRequest prep5(drvID, FsPath(L"movedir/test1.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::TreeAccessRequest prep6(drvID, FsPath(L"movedir/test2.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::TreeAccessRequest prep7(drvID, FsPath(L"movedir/test3.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep8(drvID, FsPath(L"movedir/test1.mp3"), L"testdata/test_binary_file.mp3", NULL);
	ConnectionSync::FilePushRequest prep9(drvID, FsPath(L"movedir/test2.txt"), L"testdata/test_file_size", NULL);
	ConnectionSync::FilePushRequest prep10(drvID, FsPath(L"movedir/test3.mp3"), L"testdata/test_binary_file_2.mp3", NULL);
	ConnectionSync::TreeAccessRequest prep11(drvID, FsPath(L"overwrite.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::TreeAccessRequest prep12(drvID, FsPath(L"case4move.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep13(drvID, FsPath(L"overwrite.mp3"), L"testdata/test_binary_file.mp3", NULL);
	ConnectionSync::FilePushRequest prep14(drvID, FsPath(L"case4move.mp3"), L"testdata/test_binary_file_2.mp3", NULL);
	bool p1ok = sync->doTreeAccessRequest(prep1);
	bool p2ok = sync->doFilePushRequest(prep2);
	bool p3ok = sync->doTreeAccessRequest(prep3);
	bool p4ok = sync->doTreeAccessRequest(prep4);
	bool p5ok = sync->doTreeAccessRequest(prep5);
	bool p6ok = sync->doTreeAccessRequest(prep6);
	bool p7ok = sync->doTreeAccessRequest(prep7);
	bool p8ok = sync->doFilePushRequest(prep8);
	bool p9ok = sync->doFilePushRequest(prep9);
	bool p10ok = sync->doFilePushRequest(prep10);
	bool p11ok = sync->doTreeAccessRequest(prep11);
	bool p12ok = sync->doTreeAccessRequest(prep12);
	bool p13ok = sync->doFilePushRequest(prep13);
	bool p14ok = sync->doFilePushRequest(prep14);

	if (!p1ok || !p2ok || !p3ok || !p4ok || !p5ok || !p6ok || !p7ok || !p8ok || !p9ok || !p10ok || !p11ok || !p12ok || !p13ok || !p14ok)
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

	//==TODO==

	//Test case 1: Move cached file
	output("Executing Test case 1...");
	bool tc1_pseudouseresult = Utils::binaryCompareFiles(tdrvletw + L":\\movefile.mp3", L"testdata/test_binary_file.mp3");
	BOOL tc1_moveresult = MoveFile(std::string(tdrvlet + ":\\movefile.mp3").c_str(), std::string(tdrvlet + ":\\move_here\\movedfile.mp3").c_str());
	bool tc1_compareresult = Utils::binaryCompareFiles(tdrvletw + L":\\move_here\\movedfile.mp3", L"testdata/test_binary_file.mp3");
	DWORD tc1_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\movefile.mp3").c_str());
	DWORD tc1_error = GetLastError();

	if (!tc1_pseudouseresult || !tc1_moveresult || !tc1_compareresult || tc1_checkAttribs != INVALID_FILE_ATTRIBUTES || tc1_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 1 failed: Move cached file");
		return false;
	}

	//Test case 2: Move directory with cached files
	output("Executing Test case 2...");
	bool tc2_pseudouseresult1 = Utils::binaryCompareFiles(tdrvletw + L":\\movedir\\test1.mp3", L"testdata/test_binary_file.mp3");
	bool tc2_pseudouseresult2 = Utils::binaryCompareFiles(tdrvletw + L":\\movedir\\test2.txt", L"testdata/test_file_size");
	BOOL tc2_moveresult = MoveFile(std::string(tdrvlet + ":\\movedir").c_str(), std::string(tdrvlet + ":\\move_here\\moveddir").c_str());
	bool tc2_compareresult1 = Utils::binaryCompareFiles(tdrvletw + L":\\move_here\\moveddir\\test1.mp3", L"testdata/test_binary_file.mp3");
	bool tc2_compareresult2 = Utils::binaryCompareFiles(tdrvletw + L":\\move_here\\moveddir\\test2.txt", L"testdata/test_file_size");
	bool tc2_compareresult3 = Utils::binaryCompareFiles(tdrvletw + L":\\move_here\\moveddir\\test3.mp3", L"testdata/test_binary_file_2.mp3");
	DWORD tc2_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\movedir").c_str());
	DWORD tc2_error = GetLastError();
	if (!tc2_pseudouseresult1 || !tc2_pseudouseresult2 || !tc2_moveresult || !tc2_compareresult1 || !tc2_compareresult2 || !tc2_compareresult3)
	{
		output("Test case 2 failed: Move cached directory");
		return false;
	}

	//Test case 3: Copy directory with subdirectories
	output("Executing Test case 3...");
	std::string tc3_cmd = "xcopy /e /i ";
	tc3_cmd.append(tdrvlet);
	tc3_cmd.append(":\\move_here ");
	tc3_cmd.append(tdrvlet);
	tc3_cmd.append(":\\everything_copied");
	int tc3_copyresult = system(tc3_cmd.c_str());
	bool tc3_compareresult1 = Utils::binaryCompareFiles(tdrvletw + L":\\everything_copied\\moveddir\\test1.mp3", L"testdata/test_binary_file.mp3");
	bool tc3_compareresult2 = Utils::binaryCompareFiles(tdrvletw + L":\\everything_copied\\moveddir\\test2.txt", L"testdata/test_file_size");
	bool tc3_compareresult3 = Utils::binaryCompareFiles(tdrvletw + L":\\everything_copied\\moveddir\\test3.mp3", L"testdata/test_binary_file_2.mp3");
	bool tc3_compareresult4 = Utils::binaryCompareFiles(tdrvletw + L":\\everything_copied\\movedfile.mp3", L"testdata/test_binary_file.mp3");
	if (tc3_copyresult != 0 || !tc3_compareresult1 || !tc3_compareresult2 || !tc3_compareresult3 || !tc3_compareresult4)
	{
		output("Test case 3 failed: Copy directory with subdirectories");
		return false;
	}

	//Test case 4: Overwriting internal move
	output("Executing Test case 4...");
	bool tc4_pseudouseresult1 = Utils::binaryCompareFiles(tdrvletw + L":\\overwrite.mp3", L"testdata/test_binary_file.mp3");
	bool tc4_pseudouseresult2 = Utils::binaryCompareFiles(tdrvletw + L":\\case4move.mp3", L"testdata/test_binary_file_2.mp3");
	BOOL tc4_moveresult = MoveFileEx(std::string(tdrvlet + ":\\case4move.mp3").c_str(), std::string(tdrvlet + ":\\overwrite.mp3").c_str(), MOVEFILE_REPLACE_EXISTING);
	bool tc4_compareresult = Utils::binaryCompareFiles(tdrvletw + L":\\overwrite.mp3", L"testdata/test_binary_file_2.mp3");
	DWORD tc4_checkAttribs = GetFileAttributesW(std::wstring(tdrvletw + L":\\case4move").c_str());
	DWORD tc4_error = GetLastError();
	if (!tc4_pseudouseresult1 || !tc4_pseudouseresult2 || !tc4_moveresult || !tc4_compareresult || tc4_checkAttribs != INVALID_FILE_ATTRIBUTES || tc4_error != ERROR_FILE_NOT_FOUND)
	{
		output("Test case 4 failed: Internal file move which overwrites the destination");
		return false;
	}

	//files should now be inactive
	bool inactivity1 = myCache->isClearOfReadWrite(FsPath(L"overwrite.mp3"), drvID);
	bool inactivity2 = myCache->isClearOfReadWrite(FsPath(L"move_here/movedfile.mp3"), drvID);
	bool inactivity3 = myCache->isClearOfReadWrite(FsPath(L"move_here/moveddir/test1.mp3"), drvID);
	bool inactivity4 = myCache->isClearOfReadWrite(FsPath(L"move_here/moveddir/test2.txt"), drvID);
	bool inactivity5 = myCache->isClearOfReadWrite(FsPath(L"move_here/moveddir/test3.mp3"), drvID);

	if (!inactivity1 || !inactivity2 || !inactivity3 || !inactivity4 || !inactivity5)
	{
		output("Some cache files were not inactive after termination of test cases");
		return false;
	}

	//=============================== END TEST - BEGIN TIDY UP ===============================

	output("Test cases completed");

	delete wrap;
	output("Drive unmapped");
	delete myCache;

	//evaluation
	//File Tree Check
	output("File Tree Check");
	testCheckFolderContent rootchecker;
	rootchecker.addCheckpoint(L"move_here", true);
	rootchecker.addCheckpoint(L"everything_copied", true);
	rootchecker.addCheckpoint(L"overwrite.mp3", false);
	ConnectionSync::TreeAccessRequest treeCheckRoot(drvID, FsPath(L"\\"), rootchecker, NULL);
	bool rootCheckRes = sync->doTreeAccessRequest(treeCheckRoot);
	if (!rootchecker.checkIfAllConditionsWereMet(false) || !rootCheckRes)
	{
		output("File Tree check failed (root level): Expected conditions were not met");
		return false;
	}

	testCheckFolderContent movedestchecker;
	movedestchecker.addCheckpoint(L"movedfile.mp3", false);
	movedestchecker.addCheckpoint(L"moveddir", true);
	ConnectionSync::TreeAccessRequest treeCheckMoveDest(drvID, FsPath(L"move_here"), movedestchecker, NULL);
	bool movedirCheckRes = sync->doTreeAccessRequest(treeCheckMoveDest);
	if (!movedestchecker.checkIfAllConditionsWereMet(false) || !movedirCheckRes)
	{
		output("File Tree check failed (folder move_here): Expected conditions were not met");
		return false;
	}

	testCheckFolderContent moveddirchecker;
	moveddirchecker.addCheckpoint(L"test1.mp3", false);
	moveddirchecker.addCheckpoint(L"test2.txt", false);
	moveddirchecker.addCheckpoint(L"test3.mp3", false);
	ConnectionSync::TreeAccessRequest treeCheckMoveDir(drvID, FsPath(L"move_here/moveddir"), moveddirchecker, NULL);
	bool moveCheckRes = sync->doTreeAccessRequest(treeCheckMoveDir);
	if (!moveddirchecker.checkIfAllConditionsWereMet(false) || !moveCheckRes)
	{
		output("File Tree check failed (folder moveddir): Expected conditions were not met");
		return false;
	}

	ConnectionSync::TreeAccessRequest treeCheckMoveDestCpy(drvID, FsPath(L"everything_copied"), movedestchecker, NULL);
	bool cpy1res = sync->doTreeAccessRequest(treeCheckMoveDestCpy);
	if (!movedestchecker.checkIfAllConditionsWereMet(false) || !cpy1res)
	{
		output("File Tree check failed (folder everything_copied): Expected conditions were not met");
		return false;
	}

	ConnectionSync::TreeAccessRequest treeCheckMoveDirCpy(drvID, FsPath(L"everything_copied/moveddir"), moveddirchecker, NULL);
	bool cpy2res = sync->doTreeAccessRequest(treeCheckMoveDirCpy);
	if (!moveddirchecker.checkIfAllConditionsWereMet(false) || !cpy2res)
	{
		output("File Tree check failed (folder everything_copied/movedir): Expected conditions were not met");
		return false;
	}

	//Delayed evaluation - check content of some files
	ConnectionSync::FilePullRequest checkpull1(drvID, FsPath(L"overwrite.mp3"), L"testdata/test_binary_file_2_back.mp3", NULL);
	bool cp1res1 = sync->doFilePullRequest(checkpull1);
	bool cp1res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_2_back.mp3", L"testdata/test_binary_file_2.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_2_back.mp3");
	if (!cp1res1 || !cp1res2)
	{
		output("Test case 4 failed: Internal file move which overwrites the destination");
		return false;
	}

	ConnectionSync::FilePullRequest checkpull2(drvID, FsPath(L"everything_copied/moveddir/test1.mp3"), L"testdata/test_binary_file_back.mp3", NULL);
	bool cp2res1 = sync->doFilePullRequest(checkpull2);
	bool cp2res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_back.mp3", L"testdata/test_binary_file.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_back.mp3");
	if (!cp2res1 || !cp2res2)
	{
		output("Test case 3 failed: Copy directory with subdirectories");
		return false;
	}

	ConnectionSync::FilePullRequest checkpull3(drvID, FsPath(L"move_here/movedfile.mp3"), L"testdata/test_binary_file_back.mp3", NULL);
	bool cp3res1 = sync->doFilePullRequest(checkpull3);
	bool cp3res2 = Utils::binaryCompareFiles(L"testdata/test_binary_file_back.mp3", L"testdata/test_binary_file.mp3");
	Utils::deleteOSfile(L"testdata/test_binary_file_back.mp3");
	if (!cp2res1 || !cp2res2)
	{
		output("Test case 1 failed: Move cached file");
		return false;
	}


	output("logoff");
	delete sync;

	output("SUCCESSFUL");
	//evaluation result
	return true;
}