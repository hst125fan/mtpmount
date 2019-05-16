
#ifdef defTestActivateHeaderInclude

#include "CommonTestProcedures.h"
#include "../mtpcache/FileCache.h"
#include <thread>
#include <utility>
#endif

defNewTest(MtpCachedAccessTest)
{
	MtpConnectionProvider::testReset_DO_NOT_USE();
	//get rid of connection provider on exit
	AutoDeletePointer<MtpConnectionProvider> autodelete(&(MtpConnectionProvider::getInstance()));
	output("Status: preparing");
	//obtain connection
	AbstractConnection* conn = getTestConnection();

	//syncronize connection
	ConnectionSync sync(conn);

	//map drive
	DecideForTestDrive d;
	int drvID = sync.mapNewDrive(d, false);

	//prepare tree
	ConnectionSync::TreeAccessRequest prep1(drvID, FsPath(L"testfile1.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::TreeAccessRequest prep2(drvID, FsPath(L"testfile2.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest prep3(drvID, FsPath(L"testfile1.mp3"), L"testdata/test_binary_file.mp3", NULL);
	ConnectionSync::FilePushRequest prep4(drvID, FsPath(L"testfile2.txt"), L"testdata/test_file_size", NULL);
	sync.doTreeAccessRequest(prep1);
	sync.doTreeAccessRequest(prep2);
	sync.doFilePushRequest(prep3);
	sync.doFilePushRequest(prep4);

	//cache file access
	FileCache* fc = new FileCache(sync, L"testdata/cache");

	std::vector<bool> successcontrol;
	successcontrol.push_back(false);
	successcontrol.push_back(false);
	successcontrol.push_back(false);
	std::pair<ConnectionSync*, FileCache*> workingstuff(&sync, fc);

	std::pair<int*, std::vector<bool>* > workingstuff2(&drvID, &successcontrol);
	std::pair< std::pair<ConnectionSync*, FileCache*>, std::pair<int*, std::vector<bool>* > > work(workingstuff, workingstuff2);

	output("Status: working");
	//begin asyncronous work
	std::thread t([work]() {
		ConnectionSync* s = work.first.first;
		FileCache* c = work.first.second;
		int drvID = *(work.second.first);
		ConnectionSync::TreeAccessRequest req(drvID, FsPath(L"testfile3.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
		s->doTreeAccessRequest(req);
		std::wstring cachefile = c->beginAccessByFileName(FsPath(L"testfile3.mp3"), drvID, true);
		std::ofstream cachefilestream(cachefile, std::ios::binary);
		std::ifstream testfile(L"testdata/test_binary_file_2.mp3", std::ios::binary);
		char cache = '\0';
		bool first = true;
		while (testfile.good() && !testfile.eof())
		{
			if (first)
			{
				first = false;
				cache = testfile.get();
				continue;
			}
			
			cachefilestream.put(cache);
			cache = testfile.get();
		}
		cachefilestream.close();
		testfile.close();
		c->endFileAccessByName(FsPath(L"testfile3.mp3"), drvID, true);
		work.second.second->at(1) = true;
		//output("Status: I ended");
	});

	std::thread t2([work]() {
		ConnectionSync* s = work.first.first;
		FileCache* c = work.first.second;
		int drvID = *(work.second.first);
		std::wstring filename = c->beginAccessByFileName(FsPath(L"testfile1.mp3"), drvID, false);
		work.second.second->at(1) = Utils::binaryCompareFiles(filename, L"testdata/test_binary_file.mp3");
		c->endFileAccessByName(FsPath(L"testfile1.mp3"), drvID, false);
		//output("Status: II ended");
	});

	std::thread t3([work]() {
		ConnectionSync* s = work.first.first;
		FileCache* c = work.first.second;
		int drvID = *(work.second.first);
		ConnectionSync::TreeAccessRequest req(drvID, FsPath(L"testfile2.txt"), ConnectionSync::DELETE_DIR_OR_FILE, NULL);
		std::wstring filename = c->beginAccessByFileName(FsPath(L"testfile2.txt"), drvID, false);
		s->enqueueTreeAccessRequest(req);
		work.second.second->at(2) = Utils::binaryCompareFiles(filename, L"testdata/test_file_size");
		c->endFileAccessByName(FsPath(L"testfile2.txt"), drvID, false);
		c->deleteCacheEntry(FsPath(L"testfile2.txt"), drvID);
		//output("Status: III ended");
	});

	std::wstring filename = fc->beginAccessByFileName(FsPath(L"testfile1.mp3"), drvID, false);
	bool comparesuccess = Utils::binaryCompareFiles(filename, L"testdata/test_binary_file.mp3");
	fc->endFileAccessByName(FsPath(L"testfile1.mp3"), drvID, false);
	//output("Status: IV ended");
	//finish asyncronous work
	t.join();
	t2.join();
	t3.join();
	delete fc;

	output("Status: checking");
	//check file system
	testCheckFolderContent tc;
	tc.addCheckpoint(L"testfile1.mp3", false);
	tc.addCheckpoint(L"testfile3.mp3", false);
	ConnectionSync::TreeAccessRequest test1(drvID, FsPath(L"\\"), tc, NULL);
	ConnectionSync::FilePullRequest test2(drvID, FsPath(L"testfile3.mp3"), L"testdata/back.mp3", NULL);
	sync.doTreeAccessRequest(test1);
	sync.doFilePullRequest(test2);
	bool lastfilecompare = Utils::binaryCompareFiles(L"testdata/back.mp3", L"testdata/test_binary_file_2.mp3");
	Utils::deleteOSfile(L"testdata/back.mp3");

	if (!tc.checkIfAllConditionsWereMet(false))
	{
		output("File tree unexpected");
		return false;
	}
	if (!lastfilecompare)
	{
		output("uploaded file corrupted");
		return false;
	}
	if (!comparesuccess)
	{
		output("downloaded file corrupted in IV");
		return false;
	}
	if (!successcontrol.at(1))
	{
		output("downloaded file corrupted in II");
		return false;
	}
	if (!successcontrol.at(2))
	{
		output("upload failed");
		return false;
	}
	return true;
}