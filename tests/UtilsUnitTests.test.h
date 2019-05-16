

#ifdef defTestActivateHeaderInclude

//define all headers  here
#include "..\mtpaccess\Utils.h"
#include <fstream>
#include <string>
#include <cassert>
#include "UtilsSyncTest.h"
#endif

defNewTest(FileClassifierTest)
{
	std::wstring in1(L"folder\\music.wav");
	std::wstring in2(L"contact.vcard");
	std::wstring in3(L"folder/file.bmp");
	bool ret = true;
	if (Utils::FILETYPE_MUSIC != Utils::classifyFileByName(in1)) { ret = false; output("Error: " + Utils::wstringToString(in1) + " not classfied as music"); }
	if (Utils::FILETYPE_CONTACT != Utils::classifyFileByName(in2)) { ret = false; output("Error: " + Utils::wstringToString(in2) + " not classfied as contact"); }
	if (Utils::FILETYPE_IMAGE != Utils::classifyFileByName(in3)) { ret = false; output("Error: " + Utils::wstringToString(in3) + " not classfied as image"); }
	return ret;
}

defNewTest(FileTimeConversionTest)
{
	FILETIME should;
	should.dwHighDateTime = 0x11223344;
	should.dwLowDateTime = 0x55667788;
	ULONGLONG input = 0x1122334455667788ULL;
	FILETIME outpu = Utils::timeConvert(input);
	if (outpu.dwHighDateTime != should.dwHighDateTime || outpu.dwLowDateTime != should.dwLowDateTime)
	{
		output("FileTimeConversionTest got " + std::to_string(outpu.dwHighDateTime) + "/" + std::to_string(outpu.dwLowDateTime));
		return false;
	}
	return true;
}

defNewTest(TokenizerTest)
{
	std::wstring in(L"token1 token2 \"to ke n 3\"");
	std::vector<std::wstring> dest = { L"token1", L"token2", L"to ke n 3" };
	std::vector<std::wstring> out;
	Utils::tokenize(in, out);
	if(dest != out)
	{
		output("Tokenizer test failed!");
		return false;
	}
	return true;
}

defNewTest(LongIntSplitTest)
{
	unsigned long long test1 = 0xFF;
	unsigned long long test2 = 0xEE000000DD;
	int test1l, test1h, test2l, test2h;
	Utils::splitLongInteger(test1, test1l, test1h);
	Utils::splitLongInteger(test2, test2l, test2h);

	if (test1l != 0xFF || test1h != 0)
	{
		output("Var test1 wrong");
		return false;
	}

	if (test2l != 0xDD || test2h != 0xEE)
	{
		output("Var test2 wrong");
		return false;
	}
	return true;
}

defNewTest(BinaryFileCompareTest)
{
	return Utils::binaryCompareFiles(L"testdata/test_binary_file.mp3", L"testdata/test_binary_file.mp3");
}

defNewTest(FileSizeTest)
{
	std::wstring filename(L"testdata/test_file_size");
	unsigned long long result = Utils::getOSFileSize(filename);
	if (result == 159)
	{
		return true;
	}
	else
	{
		output("Expected 159 Bytes got " + std::to_string(result) + " Byte");
		return false;
	}
}

defNewTest(CaseInsensitiveComparisonTest)
{
	std::wstring inputActCase = L"This sTring iS foR tesTing case insensItive comparisoN";
	std::wstring inputWrnCase = L"thiS sTring Is fOr teSTIng CASE inseNsitive compaRISon";
	std::wstring inputUppCase = L"THIS STRING IS FOR TESTING CASE INSENSITIVE COMPARISON";
	std::wstring inputLwrCase = L"this string is for testing case insensitive comparison";
	CaseInsensitiveWstring caseInsensitiveString(inputActCase);
	if (caseInsensitiveString == inputWrnCase &&
		caseInsensitiveString == inputUppCase &&
		caseInsensitiveString == inputLwrCase)
	{
		return true;
	}
	else
	{
		output("String insensitive comparison failed!");
		return false;
	}
}

defNewTest(FileEndingTruncationTest)
{
	std::wstring fullName1(L"test.mp3");
	std::wstring fullName2(L"test.mp3.torrent");
	std::wstring truncName1(L"test");
	std::wstring truncName2(L"test.mp3");
	if (truncName1 == Utils::getFileNameWithoutEnding(fullName1) &&
		truncName2 == Utils::getFileNameWithoutEnding(fullName2))
	{
		return true;
	}
	else
	{
		output("File Ending Truncation test failed!");
		return false;
	}
}

defNewTest(FileDeletionTest)
{
	std::wstring filename(L"testdata/file_for_deletion_testing");
	std::ofstream fileWrite(filename);
	fileWrite << "The content of this file does not really matter";
	fileWrite.close();
	std::ifstream fileReadCheck1(filename);
	if (!fileReadCheck1.good())
	{
		output("File deletion test: File write failed!");
		return false;
	}
	fileReadCheck1.close();

	Utils::deleteOSfile(filename);
	
	std::ifstream fileReadCheck2(filename);
	if (!fileReadCheck2.good())
	{
		return true;
	}
	else
	{
		output("File deletion test failed: could not delete file!");
		return false;
	}
}


defNewTest(HashProviderTest)
{
	ConflictlessHashProvider hp;
	std::wifstream stuff("testdata/hashtest");
	int lines = 0;
	while (stuff.good())
	{
		std::wstring seed;
		std::getline(stuff, seed);
		hp.getHash(seed);
		lines++;
	}
	output("HashProviderTest: Conflicts occured:" + std::to_string(hp.dbgGetConflicts()));
	if (hp.dbgGetConflicts() <= (lines/30))
	{
		return true;
	}
	return false;
}

defNewTest(DriveAvabilityTest)
{

	std::string drives = Utils::getAvailableDrives();
	//I have never seen a windows machine without drive C:, so...
	size_t forbidden1 = drives.find('C');
	size_t forbidden2 = drives.find('c');
	if (forbidden1 != std::string::npos || forbidden2 != std::string::npos)
	{
		output("C was found as free drive letter. This cannot be true!");
		return false;
	}
	return true;
}

defNewTest(DirectoryToolsTest)
{
	bool istrue1 = Utils::checkDirectoryExistance(L"testdata");
	bool isfalse1 = Utils::checkDirectoryExistance(L"testdata\\directory_does_not_exist");
	bool isfalse2 = Utils::isDirectoryEmpty(L"testdata");
	bool isfalse3 = Utils::isDirectoryEmpty(L"testdata\\directory_does_not_exist");
	bool isfalse4 = Utils::createOSDirectory(L"testdata\\test_file_size");
	bool istrue2 = Utils::createOSDirectory(L"testdata\\dirtest");
	bool istrue3 = Utils::checkDirectoryExistance(L"testdata\\dirtest");
	bool istrue4 = Utils::isDirectoryEmpty(L"testdata\\dirtest");
	std::ofstream createFile("testdata\\dirtest\\testfile");
	createFile << "some file content that does not really matter";
	createFile.close();
	bool isfalse5 = Utils::isDirectoryEmpty(L"testdata\\dirtest");
	bool istrue5 = Utils::deleteAllOSDirCont(L"testdata\\dirtest");
	bool istrue6 = Utils::isDirectoryEmpty(L"testdata\\dirtest");
	bool istrue7 = Utils::checkDirectoryExistance(L"testdata\\dirtest");
	system("rmdir /s /q testdata\\dirtest");
	
	return (istrue1 && istrue2 && istrue3 && istrue4 && istrue5 && istrue6 && istrue7 && !isfalse1 && !isfalse2 && !isfalse3 && !isfalse4 && !isfalse5) ? true : false;
}

defNewTest(ParalellSyncerTest)
{
	std::string res("teststring1");
	ParalellReadUniqueWriteSync syncer;
	std::atomic<int> reads = 0;
	std::atomic<int> writes = 0;
	assert(reads >= 0);
	assert(writes >= 0);
	assert(writes <= 1);
	assert(reads == 0 || writes == 0);

	testCritResource tr1;
	testCritResource tr2;
	testCritResource tr3;
	testCritResource tr4;
	testCritResource tr5;
	tr1.actualResource = tr2.actualResource = tr3.actualResource = tr4.actualResource = tr5.actualResource = &res;
	tr1.syncer = tr2.syncer = tr3.syncer = tr4.syncer = tr5.syncer = &syncer;
	tr1.paralellWrites = tr2.paralellWrites = tr3.paralellWrites = tr4.paralellWrites = tr5.paralellWrites = &writes;
	tr1.paralellReads = tr2.paralellReads = tr3.paralellReads = tr4.paralellReads = tr5.paralellReads = &reads;
	
	tr2.inOrBackWrite = "teststring2";
	tr3.inOrBackWrite = "teststring3";
	tr1.sleeptime = 10;
	tr2.sleeptime = 20;
	tr3.sleeptime = 40;
	tr4.sleeptime = 10;
	tr5.sleeptime = 0;

	std::thread t1(troutine1,&tr1);
	std::thread t2(troutine2,&tr2);
	std::thread t3(troutine2,&tr3);
	std::thread t4(troutine1,&tr4);
	std::thread t5(troutine1,&tr5);

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();
	output("T1: " + tr1.inOrBackWrite);
	output("T4: " + tr4.inOrBackWrite);
	output("T5: " + tr5.inOrBackWrite);
	output("F:  " + res);
	return true;
}