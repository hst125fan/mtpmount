#ifdef defTestActivateHeaderInclude

//define all headers  here
#include "CommonTestProcedures.h"
#include <fstream>
#include "..\mtpaccess\Utils.h"

#endif

defNewTest( MtpAccessBasicRoundtripTest )
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	//obtain drive
	AbstractMappableDrive* drv = getTestMapDrive();
	if (drv == NULL)
	{
		output("TestDrive could not be obtained for MtpAccessBasicRoundtripTest");
		return false;
	}

	//1. transfer PC->DEVICE
	unsigned long long filesize( Utils::getOSFileSize(L"testdata/test_binary_file.mp3"));
	std::ifstream testfile_in("testdata/test_binary_file.mp3", std::ios::binary);
	if (filesize < 1 || !testfile_in.good())
	{
		output("Test file could not be found on PC's hard drive");
		return false;
	}

	FsPath root(L"\\");
	if (!drv->createFileWithStream(root, L"simple_transfer_test.mp3", testfile_in, filesize))
	{
		output("Outbound file transfer failed");
		return false;
	}

	//2. check DEVICE
	testCheckFolderContent tsearcher;
	tsearcher.addCheckpoint(L"simple_transfer_test.mp3", false);
	drv->doForAllDirContent(root, tsearcher);
	if (!( tsearcher.checkIfAllConditionsWereMet(false)))
	{
		output("File not found on device");
		return false;
	}
	
	//3. transfer DEVICE->PC
	std::wstring testbackfile(L"testdata/test_binary_file_back.mp3");
	std::ofstream fileToPC("testdata/test_binary_file_back.mp3",std::ios::binary);
	FsPath fileOnDevice(L"simple_transfer_test.mp3");
	if (!drv->readFile(fileOnDevice, fileToPC))
	{
		output("Inbound file transfer failed");
		Utils::deleteOSfile(testbackfile);
		return false;
	}
	fileToPC.close();

	//4. delete file on DEVICE
	if (!drv->deleteFileOrDirectory(FsPath(L"simple_transfer_test.mp3"), true))
	{
		output("File deletion failed");
		return false;
	}

	testCheckFolderContent checkIfEmpty;
	drv->doForAllDirContent(root, checkIfEmpty);
	if (!checkIfEmpty.checkIfAllConditionsWereMet(false))
	{
		output("Device unempty on exit");
		return false;
	}

	bool ok = Utils::binaryCompareFiles(L"testdata/test_binary_file.mp3", L"testdata/test_binary_file_back.mp3");
	if (!ok) { output("Retrieved file differs from sent file"); }

	Utils::deleteOSfile(testbackfile);
	return ok;
}


defNewTest(MtpAccessDirectoryTreeTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	//obtain drive
	AbstractMappableDrive* drv = getTestMapDrive();
	if (drv == NULL)
	{
		output("TestDrive could not be obtained");
		return false;
	}

	//create some folders on root layer
	FsPath root(L"\\");
	if (!drv->createFolder(root, L"testfolder1") ||
		!drv->createFolder(root, L"testfolder2") ||
		!drv->createFolder(root, L"testfolder3"))
	{
		output("Folder creation on root failed");
		return false;
	}

	//create subfolder
	FsPath testfolder1(L"testfolder1");
	if (!drv->createFolder(testfolder1, L"subfolder1"))
	{
		output("Subfolder creation failed");
		return false;
	}

	//check existance
	testCheckFolderContent checkRoot;
	checkRoot.addCheckpoint(L"testfolder1",true);
	checkRoot.addCheckpoint(L"testfolder2",true);
	checkRoot.addCheckpoint(L"testfolder3",true);
	testCheckFolderContent checkTestfolder1;
	checkTestfolder1.addCheckpoint(L"subfolder1",true);

	drv->doForAllDirContent(root, checkRoot);
	drv->doForAllDirContent(testfolder1, checkTestfolder1);

	if (!checkRoot.checkIfAllConditionsWereMet(true) ||
		!checkTestfolder1.checkIfAllConditionsWereMet(true))
	{
		output("Directories are not existing on device");
		return false;
	}

	//delete some
	if (!drv->deleteFileOrDirectory(FsPath(L"testfolder3"),false) ||
		!drv->deleteFileOrDirectory(FsPath(L"testfolder1"),false)
		)
	{
		output("deletion failed");
		return false;
	}

	//check again
	testCheckFolderContent lastCheck;
	lastCheck.addCheckpoint(L"testfolder2", true);
	drv->doForAllDirContent(root, lastCheck);
	if (!lastCheck.checkIfAllConditionsWereMet(false))
	{
		output("unexpected tree after deletion");
		return false;
	}
	return true;
}

defNewTest(DirOrFileExistanceCheckTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	//obtain drive
	AbstractMappableDrive* drv = getTestMapDrive();
	if (drv == NULL)
	{
		output("TestDrive could not be obtained");
		return false;
	}

	//create a folder on root layer
	FsPath root(L"\\");
	if (!drv->createFolder(root, L"testfolder1"))
	{
		output("Folder creation on root failed");
		return false;
	}
	if (drv->checkIfExists(FsPath(L"testfolder1")) != AbstractMappableDrive::IS_DIRECTORY)
	{
		output("Folder not recognized as directory");
		return false;
	}
	return true;
}

defNewTest(FileOverwriteTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));

	//obtain drive
	AbstractMappableDrive* drv = getTestMapDrive();
	if (drv == NULL)
	{
		output("TestDrive could not be obtained");
		return false;
	}

	//transfer a file to the device, that we can overwrite later
	FsPath root(L"\\");
	std::ifstream inputfile(L"testdata/test_binary_file.mp3",std::ios::binary);
	if (!inputfile.good())
	{
		output("Input test file on PC not found");
		return false;
	}
	if (!drv->createFileWithStream(root, L"overwrite.mp3", inputfile, Utils::getOSFileSize(L"testdata/test_binary_file.mp3")) ||
		drv->checkIfExists(FsPath(L"overwrite.mp3")) != AbstractMappableDrive::IS_FILE)
	{
		output("Initial file creation failed");
		return false;
	}
	inputfile.close();
	
	std::ifstream overwritefile("testdata/test_binary_file_2.mp3",std::ios::binary);
	//overwrite it
	if (!drv->writeFile(FsPath(L"overwrite.mp3"), overwritefile, Utils::getOSFileSize(L"testdata/test_binary_file_2.mp3")))
	{
		output("File overwriting failed");
		return false;
	}
	overwritefile.close();

	//check if we have the right file
	std::ofstream out("testdata/test_binary_file_2_back.mp3", std::ios::binary);
	if (!drv->readFile(FsPath(L"overwrite.mp3"), out))
	{
		output("File read from device failed");
		return false;
	}
	out.close();

	bool ok = Utils::binaryCompareFiles(L"testdata/test_binary_file_2.mp3", L"testdata/test_binary_file_2_back.mp3");
	if (!ok)
	{
		output("File content of reaquired file did not met expectation");
	}

	Utils::deleteOSfile(L"testdata/test_binary_file_2_back.mp3");
	return ok;
}
