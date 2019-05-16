#pragma once
void threadRoutine1(std::pair<ConnectionSync*,int> p)
{
	ConnectionSync* s = p.first;
	int drvID = p.second;
	WaitOnResult w1;
	WaitOnResult w2;
	ConnectionSync::FilePullRequest pull1(drvID, FsPath(L"testdir/test2.txt"), L"testdata/test_file_size_back", &w1);
	ConnectionSync::FilePullRequest pull2(drvID, FsPath(L"test1.mp3"), L"testdata/test_binary_file_back.mp3", &w2);
	s->enqueueFilePullRequest(pull1);
	s->enqueueFilePullRequest(pull2);
	if (!w1.wait()) { /*output("Failed 1/pull1")*/ }
	if (!w2.wait()) { /*output("Failed 1/pull2")*/ }
}

void threadRoutine2(std::pair<ConnectionSync*, int> p)
{
	ConnectionSync* s = p.first;
	int drvID = p.second;
	WaitOnResult w1;
	WaitOnResult w2;
	WaitOnResult w3;
	WaitOnResult w4;
	ConnectionSync::TreeAccessRequest push1p(drvID, FsPath(L"testdir/test3.mp3"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest push1(drvID, FsPath(L"testdir/test3.mp3"), L"testdata/test_binary_file_2.mp3", &w1);
	ConnectionSync::TreeAccessRequest push2(drvID, FsPath(L"testdir/x"), ConnectionSync::CREATE_AS_DIRECTORY, &w2);
	ConnectionSync::TreeAccessRequest push3p(drvID, FsPath(L"testdir/x/test4.txt"), ConnectionSync::CREATE_AS_FILE, NULL);
	ConnectionSync::FilePushRequest push3(drvID, FsPath(L"testdir/x/test4.txt"), L"testdata/test_file_size", &w3);
	ConnectionSync::FilePushRequest push4(drvID, FsPath(L"test1.mp3"), L"testdata/test_binary_file_2.mp3", &w4);
	s->enqueueTreeAccessRequest(push1p);
	s->enqueueFilePushRequest(push1);
	s->enqueueTreeAccessRequest(push2);
	s->enqueueTreeAccessRequest(push3p);
	s->enqueueFilePushRequest(push3);
	s->enqueueFilePushRequest(push4);
	if (!w1.wait()) { /*output( "Failed 2/push1" )*/ }
	if (!w2.wait()) {/* output("Failed 2/push2" )*/ }
	if (!w3.wait()) { /*output("Failed 2/push3" )*/ }
	if (!w4.wait()) { /*output("Failed 2/push4")*/ }
}

class CheckForTestFile : public DoForAllDirContentFuctionObj
{
private:
	bool found;
public:
	CheckForTestFile() { found = false; }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
	{
		if (!currentNode->isDirectory() && currentNode->getNodeName() == L"testfile.mp3")
		{
			found = true;
		}
		return true;
	}
	bool isFound() { return found; }
};