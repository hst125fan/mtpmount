#pragma once
#include "..\mtpaccess\AbstractConnection.h"
#include <queue>
#include <mutex>
#include <map>
#include <vector>
#include <Windows.h>
#include <condition_variable>
#include <iostream>

class ConnectionSync;

void ConnectionSyncThreadRoutine(ConnectionSync* c);

class ConnectionSync
{
public:
	enum TreeAccessCmdType
	{
		INTERNAL_CREATE_AS_FILE = 0,
		INTERNAL_CREATE_AS_DIRECTORY = 1,
		INTERNAL_DELETE_DIR_OR_FILE = 2,
		INTERNAL_DISCOVER_DIRECTORY = 3,
		INTERNAL_DIR_EXISTANCE = 4,
		INTERNAL_FILE_EXISTANCE = 5,
		INTERNAL_DISCOVER_INODE = 6,
		INTERNAL_RENAME_INODE = 7,
		INTERNAL_MOVE_INODE = 8
	};
	enum UserTreeAccessCmdType
	{
		CREATE_AS_FILE = 0,
		CREATE_AS_DIRECTORY = 1,
		DELETE_DIR_OR_FILE = 2,
		CHECK_DIR_EXISTANCE = 3,
		CHECK_FILE_EXISTANCE = 4,
	};
	class RequestCallback
	{
	public:
		//This method will be called (from a different thread!!!) when the attatched request is completed
		virtual void callback(bool successful) = 0;
	};
	class FilePullRequest
	{
	private:
		int _drv;
		FsPath _dFile;
		std::wstring _pFile;
		RequestCallback* _callback;
		friend class ConnectionSync;
	public:
		//callback parameter is not copied. On asyncronous calls, ensure it doesn't get deleted!
		FilePullRequest(int driveId, const FsPath& fileOnDevice, const std::wstring& fileOnPc = L"", RequestCallback* callback = NULL);
	};
	class FilePushRequest
	{
	private:
		int _drv;
		FsPath _dFile;
		std::wstring _pFile;
		RequestCallback* _callback;
		friend class ConnectionSync;
	public:
		//callback parameter is not copied. On asyncronous calls, ensure it doesn't get deleted!
		FilePushRequest(int driveId, const FsPath& fileOnDevice, const std::wstring& fileOnPc = L"", RequestCallback* callback = NULL);
	};
	class TreeAccessRequest
	{
	private:
		int _drv;
		FsPath _dir;
		std::wstring _dir2;
		RequestCallback* _callback;
		DoForAllDirContentFuctionObj* _dirfunc;
		TreeAccessCmdType _type;
		friend class ConnectionSync;
	public:
		//Constructor only for directory discovery!
		//funcobj and callback parameters are not copied. On asyncronous calls, ensure they don't get deleted!
		TreeAccessRequest(int driveId, const FsPath& deviceDirectory, DoForAllDirContentFuctionObj& funcobj, RequestCallback* callback = NULL);

		//Constructor for single-node discovery
		//funcobj and callback parameters are not copied. On asyncronous calls, ensure they don't get deleted!
		//first parameter int constructdiscriminator is not used.
		TreeAccessRequest(int constructdiscriminator,int driveId, const FsPath& deviceInode, DoForAllDirContentFuctionObj& funcobj, RequestCallback* callback = NULL);

		//Constructor for all other purposes than directory discovery
		//callback parameter is not copied. On asyncronous calls, ensure it doesn't get deleted!
		TreeAccessRequest(int driveId, const FsPath& devicePath, UserTreeAccessCmdType type, RequestCallback* callback = NULL);

		//Special constructor for node renaming
		//callback parameter is not copied. On asyncronous calls, ensure it doesn't get deleted!
		TreeAccessRequest(int driveId, const FsPath& devicePath, const std::wstring& newName, RequestCallback* callback = NULL);

		//Special constructor for node moving
		//callback parameter is not copied. On asyncronous calls, ensure it doesn't get deleted!
		TreeAccessRequest(int driveId, const FsPath& devicePath, FsPath& newDevicePath, RequestCallback* callback = NULL);

	};
	class MapDriveDecider
	{
	public:
		//Drive decision method
		//Parameter: a string vector with all possible drives to add
		//Return: index in vector. -1 to cancel.
		virtual int decideDrive(std::vector<std::wstring>& availableDrives) = 0;
	};
	
private:
	std::queue<TreeAccessRequest> _treeAccessRequests; //PRIO 1
	std::queue<FilePullRequest> _filePullRequests; //PRIO 2
	std::queue<FilePushRequest> _filePushRequests; //PRIO 3
	std::mutex* _filePullRequestsMutex;
	std::mutex* _treeAccessRequestsMutex;
	std::mutex* _filePushRequestsMutex;
	std::mutex _threadStopCondVarMutex;
	std::condition_variable _threadStopCondVar;
	bool _threadStopCondVarUnlocked;
	typedef std::map<int, AbstractMappableDrive*> myDrives_type;
	myDrives_type _myDrives;
	int _myDrivesCounter;
	AbstractConnection* _myConnection;
	bool _threadNormalStop;
	std::thread* myThread;
	friend void ConnectionSyncThreadRoutine(ConnectionSync* c);
	bool _threadHandleTreeAccessReq();
	bool _threadHandleFilePullReq();
	bool _threadHandleFilePushReq();
	void _threadYield();
	void _threadWakeup();
	std::ostream* _outputter;
	std::mutex _outputterLock;
public:
	ConnectionSync(AbstractConnection* conn);
	~ConnectionSync();
	//returncode = drive identifier for request objects
	int mapNewDrive(MapDriveDecider& decider, bool fullRefresh = false);
	bool unmapDrive(int id);
	//asyncronous
	void enqueueFilePullRequest(const FilePullRequest& request);
	//asyncronous
	void enqueueFilePushRequest(const FilePushRequest& request);
	//asyncronous
	void enqueueTreeAccessRequest(const TreeAccessRequest& request);
	//syncronized
	bool doFilePullRequest(FilePullRequest& request);
	//syncronized
	bool doFilePushRequest(FilePushRequest& request);
	//syncronized
	bool doTreeAccessRequest(TreeAccessRequest& request);
	void setOutputter(std::ostream* outputter);
};


class WaitOnResult : public ConnectionSync::RequestCallback
{
private:
	std::condition_variable condvar;
	std::mutex mtx;
	bool unlocked;
	bool success;
public:
	WaitOnResult()
	{
		unlocked = false;
		success = false;
	}
	virtual void callback(bool successful)
	{
		success = successful;
		unlocked = true;
		condvar.notify_one();
	}
	bool wait()
	{
		std::unique_lock<std::mutex> lck(mtx);
		while (!unlocked)
		{
			condvar.wait(lck);
		}
		return success;
	}
};