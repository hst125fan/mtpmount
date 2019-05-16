#include "../dokanconnect/MemLeakDetection.h"
#include "FileCache.h"


FileCache::FileCache(ConnectionSync& onThisConnection)
{
	_transferInfo.useThisConnection = &onThisConnection;
	_interruptBackgroundReadOperations = false;
	_interruptBackgroundWriteOperations = false;
	_transferInfo.cacheBaseDir = CacheLocationProvider::getInstance().getCacheLocation();
}

FileCache::FileCache(ConnectionSync& onThisConnection, std::wstring /*cacheDir*/) : FileCache(onThisConnection) { }


FileCache::~FileCache()
{
	_cachemapLock.getWriteAccess();
	for (typeof_cachemap::iterator iter = _cachemap.begin();
		iter != _cachemap.end();
		iter++)
	{
		delete (iter->second);
	}
	_cachemapLock.endWriteAccess();
}

void FileCache::deleteAllIdles()
{
	//This code is dangerous!
	/*_cachemapLock.getWriteAccess();
	for (typeof_cachemap::iterator iter = _cachemap.begin();
		iter != _cachemap.end();
		iter++)
	{
		if (iter->second->isIdle())
		{
			delete (iter->second);
			_cachemap.erase(iter);
		}
		if (_interruptBackgroundWriteOperations) { break; }
	}
	_cachemapLock.endWriteAccess();*/
}

void FileCache::syncAllDirties()
{
	_cachemapLock.getReadAccess();
	for (typeof_cachemap::iterator iter = _cachemap.begin();
		iter != _cachemap.end();
		iter++)
	{
		if (iter->second->isDirty())
		{
			iter->second->pushIfDirty();
		}
		if (_interruptBackgroundReadOperations) { break; }
	}
	_cachemapLock.endReadAccess();
}

void FileCache::generalTimeBasedSync()
{
	std::list<FileTableKey> deleteThese;
	_cachemapLock.getReadAccess();
	for (typeof_cachemap::iterator iter = _cachemap.begin();
		iter != _cachemap.end();
		iter++)
	{
		if (iter->second->isDirty() && iter->second->writeAccessClear() && iter->second->getLastWriteTimeDiff() > 15.0)
		{
			iter->second->pushIfDirty();
		}
		if (iter->second->isIdle() && iter->second->accessClear() && iter->second->getLastReadTimeDiff() > 30.0 && iter->second->getLastWriteTimeDiff() > 30.0)
		{
			deleteThese.push_back(iter->first);
		}
		if (_interruptBackgroundReadOperations) { break; }
	}
	_cachemapLock.endReadAccess();

	//got all objects to delete, now do something about it!
	_cachemapLock.getWriteAccess();
	for (std::list<FileTableKey>::iterator iter = deleteThese.begin(); iter != deleteThese.end(); iter++)
	{
		try
		{
			FileTableEntry* e = _cachemap.at(*iter);
			delete e;
			_cachemap.erase(*iter);
		}
		catch (std::out_of_range)
		{
			//no additional action needed
		}
		if (_interruptBackgroundWriteOperations) { break; }
	}
	_cachemapLock.endWriteAccess();
}

int FileCache::getTotalCachedFileCount(bool prio)
{
	if (prio) { _interruptBackgroundWriteOperations = true; }
	_cachemapLock.getReadAccess();
	if (prio) { _interruptBackgroundWriteOperations = false; }
	int totalFileCount = static_cast<int>( _cachemap.size() );
	_cachemapLock.endReadAccess();
	return totalFileCount;
}

int FileCache::getDirtyFilesCount(bool prio)
{
	if (prio) { _interruptBackgroundWriteOperations = true; }
	_cachemapLock.getReadAccess();
	if (prio) { _interruptBackgroundWriteOperations = false; }
	int counter = 0;
	for (typeof_cachemap::iterator iter = _cachemap.begin();
		iter != _cachemap.end();
		iter++)
	{
		if (iter->second->isDirty())
		{
			counter++;
		}
	}
	_cachemapLock.endReadAccess();
	return counter;
}


FileCache::FileTableEntry* FileCache::_createCacheEntry(FsPath toThisFile, int onThisDrive)
{
	FileTableEntry* entry = NULL;
	_interruptBackgroundReadOperations = true;
	_interruptBackgroundWriteOperations = true;
	_cachemapLock.getWriteAccess();
	try
	{
		entry = _cachemap.at(FileTableKey(onThisDrive, toThisFile));
	}
	catch (std::out_of_range)
	{
		entry = new FileTableEntry(toThisFile, onThisDrive, *this);
		_cachemap.emplace(typeof_cachemap::value_type(FileTableKey(onThisDrive, toThisFile), entry));
	}
	_interruptBackgroundReadOperations = false;
	_interruptBackgroundWriteOperations = false;
	_cachemapLock.endWriteAccess();
	return entry;
}

FileCache::FileTableEntry::FileTableEntry(FsPath devicePath, int drvId, FileCache& creator) : _pathOnDevice(devicePath),
																							  _parent(&creator),
																							  _drvIdOnDevice(drvId),
																							  _readCallback(this),
																							  _writeCallback(this),
																							  _deleteWithoutSync(false)
{
	_pathInCache = _parent->useHasher().getHash(devicePath.getFullPath());
	_status = CACHE_NONEXISTANT;
	_readAccesses = 0;
	_writeAccesses = 0;
}

void FileCache::FileTableEntry::_asyncPull()
{
	_statusMutex.lock();
	if (_status == CACHE_PULLING)
	{
		_statusMutex.unlock();
		return;
	}
	_status = CACHE_PULLING;
	_statusMutex.unlock();
	ConnectionSync::FilePullRequest req(_drvIdOnDevice, _pathOnDevice, _parent->_transferInfo.cacheBaseDir + L"\\" + _pathInCache, &_readCallback);
	_parent->getTransferInfo().useThisConnection->enqueueFilePullRequest(req);
}

void FileCache::FileTableEntry::_asyncPush()
{
	_statusMutex.lock();
	if (_status == CACHE_PUSHING)
	{
		_statusMutex.unlock();
		return;
	}
	_status = CACHE_PUSHING;
	_statusMutex.unlock();
	ConnectionSync::FilePushRequest req(_drvIdOnDevice, _pathOnDevice, _parent->_transferInfo.cacheBaseDir + L"\\" + _pathInCache, &_writeCallback);
	_parent->getTransferInfo().useThisConnection->enqueueFilePushRequest(req);
}

bool FileCache::FileTableEntry::_getAccess(bool ignorePush)
{
	_statusMutex.lock();
	if (_status == CACHE_PUSHING && !ignorePush)
	{
		std::mutex waitMtx;
		std::unique_lock<std::mutex> waitLck(waitMtx);
		_statusMutex.unlock();
		while (_status == CACHE_PUSHING)
		{
			_avabilityNotifier.wait(waitLck);
		}
		return true;
	}
	else if (_status == CACHE_AVAILABLE || _status == CACHE_PUSHING || _status == CACHE_DIRTY)
	{
		_statusMutex.unlock();
		return true;
	}
	else
	{
		_statusMutex.unlock();
		_asyncPull();
		std::mutex waitMtx;
		std::unique_lock<std::mutex> waitLck(waitMtx);
		while (_status == CACHE_PULLING)
		{
			_avabilityNotifier.wait(waitLck);
		}

		if (_status != CACHE_FAIL)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool FileCache::FileTableEntry::getReadAccess()
{
	_readAccesses++;
	_lastRead.makeNow();
	return _getAccess();
}

void FileCache::FileTableEntry::endReadAccess()
{
	_readAccesses--;
	_lastRead.makeNow();
}

bool FileCache::FileTableEntry::getWriteAccess()
{
	_writeAccesses++;
	_lastWrite.makeNow();
	return _getAccess(false);
}

void FileCache::FileTableEntry::endWriteAccess()
{
	_writeAccesses--;
	_lastWrite.makeNow();
	_statusMutex.lock();
	_status = CACHE_DIRTY;
	_statusMutex.unlock();
}

std::wstring FileCache::FileTableEntry::getCachePath()
{
	return _parent->_transferInfo.cacheBaseDir + L"\\" + _pathInCache;
}

bool FileCache::FileTableEntry::remap(FileCache& parent, int drvId, bool force)
{
	if (!force && _status != CACHE_PULLING && _status != CACHE_PUSHING)
	{
		_parent = &parent;
		_drvIdOnDevice = drvId;
		return true;
	}
	else
	{
		return false;
	}
}

bool FileCache::FileTableEntry::isIdle()
{
	return (_status != CACHE_PULLING &&
		_status != CACHE_PUSHING &&
		_status != CACHE_DIRTY &&
		_writeAccesses == 0 &&
		_readAccesses == 0) ? true : false;
}

bool FileCache::FileTableEntry::isDirty()
{
	return (_status == CACHE_DIRTY);
}

bool FileCache::FileTableEntry::accessClear()
{
	return (_readAccesses == 0 && _writeAccesses == 0);
}

bool FileCache::FileTableEntry::writeAccessClear()
{
	return (_writeAccesses == 0);
}

void FileCache::FileTableEntry::pushIfDirty()
{
	if (_status == CACHE_DIRTY)
	{
		_asyncPush();
	}
}

void FileCache::AvabilitySwitcher::callback(bool successful)
{
	_fte->_statusMutex.lock();
	if (!successful)
	{
		_fte->_status = FileCache::CACHE_FAIL;
	}
	else
	{
		_fte->_status = FileCache::CACHE_AVAILABLE;
	}
	_fte->_statusMutex.unlock();
	_fte->_avabilityNotifier.notify_all();
}

void FileCache::AvabilitySwitcher2::callback(bool successful)
{
	_fte->_statusMutex.lock();
	if (_fte->_status == CACHE_PUSHING)
	{ 
		if (successful)
		{
			_fte->_status = CACHE_AVAILABLE;
		}
		else
		{
			_fte->_status = CACHE_DIRTY;
		}
	}
	_fte->_statusMutex.unlock();
	_fte->_avabilityNotifier.notify_all();
}

FileCache::FileTableEntry::~FileTableEntry()
{
	if (!_deleteWithoutSync)
	{
		pushIfDirty();
		std::mutex waitMtx;
		std::unique_lock<std::mutex> waitLck(waitMtx);
		while (_status == CACHE_PUSHING)
		{
			_avabilityNotifier.wait(waitLck);
		}
	}
	Utils::deleteOSfile(_parent->_transferInfo.cacheBaseDir + L"\\" + _pathInCache);
}


FileCache::FileTableEntry* FileCache::_cacheAccess(FsPath file, int drvID)
{
	FileTableEntry* fte;
	_interruptBackgroundWriteOperations = true;
	_cachemapLock.getReadAccess();
	_interruptBackgroundWriteOperations = false;
	try
	{
		fte = _cachemap.at(FileTableKey(drvID,file));
		_cachemapLock.endReadAccess();
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endReadAccess();
		fte = _createCacheEntry(file, drvID); //this cares about write access itself
	}
	return fte;
}

std::wstring FileCache::beginAccessByFileName(FsPath path, int drv, bool write)
{
	FileTableEntry* fte = _cacheAccess(path, drv);
	if (write)
	{
		if (!fte->getWriteAccess()) { return L"ERROR"; }
		else { return fte->getCachePath(); }
	}
	else
	{
		if (!fte->getReadAccess()) { return L"ERROR"; }
		else { return fte->getCachePath(); }
	}
}

void FileCache::endFileAccessByName(FsPath path, int drv, bool write)
{
	FileTableEntry* fte = _cacheAccess(path, drv);
	if (write)
	{
		fte->endWriteAccess();
	}
	else
	{
		fte->endReadAccess();
	}
}

bool FileCache::deleteCacheEntry(FsPath path, int drvId)
{
	_interruptBackgroundReadOperations = true;
	_interruptBackgroundWriteOperations = true;
	_cachemapLock.getWriteAccess();
	_interruptBackgroundReadOperations = false;
	_interruptBackgroundWriteOperations = false;
	try
	{
		FileTableKey k(drvId, path);
		_cachemap.at(k)->setDeleteWithoutSync();
		delete _cachemap.at(k);
		_cachemap.erase(k);
		_cachemapLock.endWriteAccess();
		return true;
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endWriteAccess();
		return false;
	}
}

bool FileCache::rebindCacheEntry(FsPath oldpath, FsPath newpath, int drvId)
{
	_interruptBackgroundReadOperations = true;
	_interruptBackgroundWriteOperations = true;
	_cachemapLock.getWriteAccess();
	_interruptBackgroundReadOperations = false;
	_interruptBackgroundWriteOperations = false;

	bool success = _unsyncedCacheRebind(oldpath, newpath, drvId);

	_cachemapLock.endWriteAccess();
	return success;
}

bool FileCache::moveOrRenameInode(FsPath oldpath, FsPath newpath, int drvId)
{
	FileTableKey k(drvId, newpath);
	try
	{
		delete _cachemap.at(k);
		_cachemap.erase(k);
	}
	catch (std::out_of_range) {/*do nothing*/ }

	bool success = false;
	if (_mustMove(oldpath, newpath))
	{
		success = moveInode(oldpath, newpath, drvId);
	}
	else
	{
		success = renameInode(oldpath, newpath.getPathOnLayer(newpath.getPathLength() - 1).getActualCase(), drvId);
	}
	if (!success) { return false; }

	_interruptBackgroundReadOperations = true;
	_interruptBackgroundWriteOperations = true;
	_cachemapLock.getWriteAccess();
	_interruptBackgroundReadOperations = false;
	_interruptBackgroundWriteOperations = false;

	ConnectionSync::TreeAccessRequest isdir(drvId, newpath, ConnectionSync::CHECK_DIR_EXISTANCE, NULL);
	if (_transferInfo.useThisConnection->doTreeAccessRequest(isdir))
	{
		CacheRebinder rebinder(*this, oldpath, newpath, drvId);
		ConnectionSync::TreeAccessRequest rebind(drvId, newpath, rebinder, NULL);
		_transferInfo.useThisConnection->doTreeAccessRequest(rebind);
	}
	else
	{
		_unsyncedCacheRebind(oldpath, newpath, drvId);
	}

	_cachemapLock.endWriteAccess();
	return true;
}

bool CacheRebinder::doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
{
	FsPath oldNodePath(oldPathOfDir.getFullPath() + L"\\" + currentNode->getNodeName());
	FsPath newNodePath(newPathOfDir.getFullPath() + L"\\" + currentNode->getNodeName());
	if (currentNode->isDirectory())
	{
		CacheRebinder rebinder(*callMe, oldNodePath, newNodePath, drvId);
		currentNode->doForAllContent(rebinder);
	}
	else
	{
		callMe->_unsyncedCacheRebind(oldNodePath, newNodePath, drvId);
	}
	return true;
}

bool FileCache::_unsyncedCacheRebind(FsPath oldpath, FsPath newpath, int drvId)
{
	FileTableKey kold(drvId, oldpath);
	FileTableKey knew(drvId, newpath);
	FileTableEntry* entry = NULL;
	try
	{
		entry = _cachemap.at(kold);
	}
	catch (std::out_of_range)
	{
		return false;
	}
	
	try
	{
		delete _cachemap.at(knew);
		_cachemap.erase(knew);
	}
	catch (std::out_of_range) {/*do nothing*/}

	_cachemap.insert(typeof_cachemap::value_type(knew, entry));
	_cachemap.erase(kold);
	entry->rename(newpath);
	return true;
}

bool FileCache::renameInode(FsPath oldname, std::wstring newname, int drvId)
{
	ConnectionSync::TreeAccessRequest req(drvId, oldname, newname, NULL);
	return _transferInfo.useThisConnection->doTreeAccessRequest(req);
}

bool FileCache::moveInode(FsPath oldname, FsPath newname, int drvId)
{
	ConnectionSync::TreeAccessRequest req(drvId, oldname, newname, NULL);
	return _transferInfo.useThisConnection->doTreeAccessRequest(req);
}

bool FileCache::_mustMove(FsPath path1, FsPath path2)
{
	if (path1.getPathLength() != path2.getPathLength())
	{
		return true;
	}

	bool different = false;
	for (int i = 0; i < path1.getPathLength() - 1; i++)
	{
		if (path1.getPathOnLayer(i) != path2.getPathOnLayer(i))
		{
			different = true;
			break;
		}
	}
	return different;
}

bool FileCache::pushSingleFile(FsPath path, int drvId)
{
	FileTableKey k(drvId, path);
	_cachemapLock.getReadAccess();
	try
	{ 
		FileTableEntry* entry = _cachemap.at(k);
		_cachemapLock.endReadAccess();
		entry->pushIfDirty();
		return true;
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endReadAccess();
		return false;
	}
}

bool FileCache::getIdleStatus(FsPath path, int drvId)
{
	FileTableKey k(drvId, path);
	_cachemapLock.getReadAccess();
	try
	{
		FileTableEntry* entry = _cachemap.at(k);
		_cachemapLock.endReadAccess();
		return entry->isIdle();
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endReadAccess();
		return false;
	}
}

bool FileCache::isClearOfWrite(FsPath path, int drvId)
{
	FileTableKey k(drvId, path);
	_cachemapLock.getReadAccess();
	try
	{
		FileTableEntry* entry = _cachemap.at(k);
		_cachemapLock.endReadAccess();
		return entry->writeAccessClear();
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endReadAccess();
		return true;
	}
}

bool FileCache::isClearOfReadWrite(FsPath path, int drvId)
{
	FileTableKey k(drvId, path);
	_cachemapLock.getReadAccess();
	try
	{
		FileTableEntry* entry = _cachemap.at(k);
		_cachemapLock.endReadAccess();
		return entry->accessClear();
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endReadAccess();
		return true;
	}
}

unsigned long long FileCache::getFileSizeOfCachedFile(FsPath inode, int drvId)
{
	FileTableKey k(drvId, inode);
	_cachemapLock.getReadAccess();
	try
	{
		FileTableEntry* entry = _cachemap.at(k);
		_cachemapLock.endReadAccess();
		return Utils::getOSFileSize(entry->getCachePath());
	}
	catch (std::out_of_range)
	{
		_cachemapLock.endReadAccess();
		return 0;
	}
	
}

bool operator<(const FileTableKey& rhs, const FileTableKey& lhs)
{
	if (rhs._drv == lhs._drv)
	{
		return rhs._filepath < lhs._filepath;
	}
	return rhs._drv < lhs._drv;
}

FileCacheBackgroundJob::FileCacheBackgroundJob(FileCache& cache, int interval, int maxDirties, int maxFiles) :
	_myCache(&cache),
	_interval(interval),
	_maxDirties(maxDirties),
	_maxFiles(maxFiles),
	_run(true)
{
	_myThread = new std::thread(FileCacheBackgroundJobThreadRoutine, this);
}

void FileCacheBackgroundJob::earlyStop()
{
	if (_myThread != NULL)
	{
		_run = false;
		_myThread->join();
		delete _myThread;
		_myThread = NULL;
	}
}

FileCacheBackgroundJob::~FileCacheBackgroundJob()
{
	if (_myThread != NULL)
	{
		_run = false;
		_myThread->join();
		delete _myThread;
		_myThread = NULL;
	}
}

void FileCacheBackgroundJobThreadRoutine(FileCacheBackgroundJob* job)
{
	while (job->_run)
	{
		Sleep(job->_interval);
		job->_myCache->generalTimeBasedSync();
		/*if (job->_maxDirties > 0 && job->_maxDirties < job->_myCache->getDirtyFilesCount())
		{
			job->_myCache->syncAllDirties();
		}
		if (job->_maxFiles > 0 && job->_maxFiles < job->_myCache->getTotalCachedFileCount() )
		{
			job->_myCache->deleteAllIdles();
		}*/
	}
}