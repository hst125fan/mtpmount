#pragma once
#include <map>
#include <string>
#include "..\mtpaccess\AbstractFileSystem.h"
#include "ConnectionSync.h"
#include <mutex>
#include <atomic>
#include <ctime>

//what should it do?
//User: "I want to read this file" -> PULL, save a local copy and give user this copy (unless: file already copied), BLOCK
//User: "I want to write this file" -> write on local copy, mark as dirty, maybe send PUSH request (low-prio - might not be executed right now), NON-BLOCK
//User: "I want to create/delete" -> Immediately send request, BLOCK
//User: "show me the files in..." -> Immediately send request, BLOCK
class FileTableKey
{
public:
	FileTableKey(const int& drv, const FsPath& filepath) : _drv(drv), _filepath(filepath) { }
	int _drv;
	FsPath _filepath;

};

bool operator<(const FileTableKey& rhs, const FileTableKey& lhs);


class FileTableTimeEntry
{
private:
	time_t _time;
	std::mutex _mtx;
public:
	FileTableTimeEntry() { makeNow(); }
	void makeNow() { _mtx.lock(); time(&_time); _mtx.unlock(); }
	double howOld() { 
		_mtx.lock();
		time_t now;
		time(&now);
		double ret = difftime(now, _time);
		_mtx.unlock();
		return ret;
	}
};

class FileCache
{
public:
	struct TransferInfo
	{
		ConnectionSync* useThisConnection;
		std::wstring cacheBaseDir;
	};
private:
	class FileTableEntry;
	class AvabilitySwitcher : public ConnectionSync::RequestCallback
	{
	private:
		FileTableEntry* _fte;
	public:
		AvabilitySwitcher(FileTableEntry* fte) : _fte(fte) { }
		virtual void callback(bool successful);
	};
	class AvabilitySwitcher2 : public ConnectionSync::RequestCallback
	{
	private:
		FileTableEntry* _fte;
	public:
		AvabilitySwitcher2(FileTableEntry* fte) : _fte(fte) { }
		virtual void callback(bool successful);
	};
	enum FileTableEntryStatus
	{
		CACHE_NONEXISTANT = 0,
		CACHE_PULLING = 1,
		CACHE_AVAILABLE = 2,
		CACHE_DIRTY = 3,
		CACHE_PUSHING = 4,
		CACHE_FAIL = 5
	};
	class FileTableEntry
	{
	private:
		FileTableEntryStatus _status;
		bool _deleteWithoutSync;
		std::mutex _statusMutex;
		FsPath _pathOnDevice;
		int _drvIdOnDevice;
		FileCache* _parent;
		std::wstring _pathInCache;
		std::atomic<int> _readAccesses;
		std::atomic<int> _writeAccesses;
		std::condition_variable _avabilityNotifier;
		AvabilitySwitcher _readCallback;
		AvabilitySwitcher2 _writeCallback;
		void _asyncPull();
		void _asyncPush();
		bool _getAccess(bool ignorePush = true);
		friend class FileCache::AvabilitySwitcher;
		friend class FileCache::AvabilitySwitcher2;
		FileTableTimeEntry _lastWrite;
		FileTableTimeEntry _lastRead;
	public:
		FileTableEntry(FsPath devicePath, int drvId, FileCache& creator);
		~FileTableEntry();
		bool getReadAccess();
		void endReadAccess();
		bool getWriteAccess();
		void endWriteAccess();
		std::wstring getCachePath();
		bool remap(FileCache& parent, int drvId, bool force = false);
		bool isIdle();
		bool isDirty();
		bool accessClear();
		bool writeAccessClear();
		void pushIfDirty();
		void setDeleteWithoutSync() { _deleteWithoutSync = true; }
		void rename(FsPath newPath) { _pathOnDevice = newPath; }
		double getLastReadTimeDiff() { return _lastRead.howOld(); }
		double getLastWriteTimeDiff() { return _lastWrite.howOld(); }
	};

	TransferInfo  _transferInfo;
	ConflictlessHashProvider _hashprov;
	typedef std::map<FileTableKey, FileTableEntry*> typeof_cachemap;
	typeof_cachemap _cachemap;
	ParalellReadUniqueWriteSync _cachemapLock;
	bool _interruptBackgroundWriteOperations; //should be set on every READ and WRITE access on cachemap
	bool _interruptBackgroundReadOperations; //should be set on every WRITE access on cachemap
	FileTableEntry* _createCacheEntry(FsPath toThisFile, int onThisDrive);
	FileTableEntry* _cacheAccess(FsPath file, int drvID);
	std::atomic<int> _dirties;
	std::atomic<int> _totalcount;
	bool _mustMove(FsPath path1, FsPath path2);
	bool _unsyncedCacheRebind(FsPath oldpath, FsPath newpath, int drvId);
	bool renameInode(FsPath oldname, std::wstring newname, int drvId);
	bool moveInode(FsPath oldname, FsPath newname, int drvId);
	friend class CacheRebinder;
public:
	FileCache(ConnectionSync& onThisConnection);
	FileCache(ConnectionSync& onThisConnection, std::wstring cacheDir);
	~FileCache();
	TransferInfo getTransferInfo() { return _transferInfo; }
	ConflictlessHashProvider& useHasher() { return _hashprov; }

	void deleteAllIdles();
	void syncAllDirties();
	void generalTimeBasedSync();

	int getTotalCachedFileCount(bool prio = false);
	int getDirtyFilesCount(bool prio = false);

	std::wstring beginAccessByFileName(FsPath path, int drvId, bool write = false);
	void endFileAccessByName(FsPath path, int drvId, bool write = false);

	bool deleteCacheEntry(FsPath path, int drvId);
	bool pushSingleFile(FsPath path, int drvId);
	bool getIdleStatus(FsPath path, int drvId);
	bool isClearOfWrite(FsPath path, int drvId);
	bool isClearOfReadWrite(FsPath path, int drvId);

	bool DEPRECATED rebindCacheEntry(FsPath oldpath, FsPath newpath, int drvId);

	bool moveOrRenameInode(FsPath oldpath, FsPath newpath, int drvId);

	unsigned long long getFileSizeOfCachedFile(FsPath inode, int drvId);

	
	//preAccessRequestReturn preAccessRequest(FsPath path, int drvId);


//add DOKAN-friendly file access interface
};

class FileCacheBackgroundJob
{
private:
	FileCache* _myCache;
	std::thread* _myThread;
	int _interval;
	int _maxDirties;
	int _maxFiles;
	bool _run;
	friend void FileCacheBackgroundJobThreadRoutine(FileCacheBackgroundJob* job);
public:
	FileCacheBackgroundJob(FileCache& cache, int intervalInMs, int maxDirties, int maxFiles);
	void earlyStop();
	~FileCacheBackgroundJob();
};

void FileCacheBackgroundJobThreadRoutine(FileCacheBackgroundJob* job);

class CacheRebinder : public DoForAllDirContentFuctionObj
{
private:
	FileCache* callMe;
	FsPath oldPathOfDir;
	FsPath newPathOfDir;
	int drvId;
public:
	CacheRebinder(FileCache& caller, FsPath oldPath, FsPath newPath, int drvID) : callMe(&caller), oldPathOfDir(oldPath), newPathOfDir(newPath), drvId(drvID) { }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode);
};
