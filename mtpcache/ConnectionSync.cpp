#include "../dokanconnect/MemLeakDetection.h"
#include "ConnectionSync.h"
#include <fstream>
#include <sstream>

ConnectionSync::ConnectionSync(AbstractConnection* conn) : _myConnection(conn), _myDrivesCounter(0)
{
	_filePullRequestsMutex = new std::mutex;
	_filePushRequestsMutex = new std::mutex;
	_treeAccessRequestsMutex = new std::mutex;
	_threadNormalStop = false;
	_threadStopCondVarUnlocked = false;
	_outputter = NULL;
	myThread = new std::thread(ConnectionSyncThreadRoutine, this);
}

void ConnectionSync::setOutputter(std::ostream* outputter)
{
	_outputterLock.lock();
	_outputter = outputter;
	_outputterLock.unlock();
}

int ConnectionSync::mapNewDrive(ConnectionSync::MapDriveDecider& decider, bool fullRefresh)
{
	std::vector<std::wstring> decision;
	_myConnection->getMappableDriveNames(decision, fullRefresh);
	int bossHasDecidedFor = decider.decideDrive(decision);
	try
	{
		AbstractMappableDrive* newdrive = _myConnection->getMDriveByFName(decision.at(bossHasDecidedFor));
		_myDrives.insert(myDrives_type::value_type(_myDrivesCounter, newdrive));
		_myDrivesCounter++;
		return _myDrivesCounter - 1;
	}
	catch (std::out_of_range)
	{
		return -1;
	}
}

bool ConnectionSync::unmapDrive(int id)
{
	try
	{
		AbstractMappableDrive* drv = _myDrives.at(id);
		_myDrives.erase(id);
		return true;
	}
	catch(std::out_of_range)
	{
		return false;
	}
}

void ConnectionSync::_threadYield()
{	
	std::mutex waitmtx;
	std::unique_lock<std::mutex> waitlck(waitmtx);
	_threadStopCondVarMutex.lock();
	while (!_threadStopCondVarUnlocked)
	{
		_threadStopCondVarMutex.unlock();
		_threadStopCondVar.wait(waitlck);
		_threadStopCondVarMutex.lock();
	}
	_threadStopCondVarUnlocked = false;
	_threadStopCondVarMutex.unlock();
}

void ConnectionSync::_threadWakeup()
{
	_threadStopCondVarMutex.lock();
	_threadStopCondVarUnlocked = true;
	_threadStopCondVarMutex.unlock();
	_threadStopCondVar.notify_one();
}


ConnectionSync::FilePushRequest::FilePushRequest(int driveId, const FsPath& fileOnDevice, const std::wstring& fileOnPc, RequestCallback* callback) :
	_drv(driveId),
	_dFile(fileOnDevice),
	_pFile(fileOnPc),
	_callback(callback)
{

}

ConnectionSync::FilePullRequest::FilePullRequest(int driveId, const FsPath& fileOnDevice, const std::wstring& fileOnPc, RequestCallback* callback) :
	_drv(driveId),
	_dFile(fileOnDevice),
	_pFile(fileOnPc),
	_callback(callback)
{

}

ConnectionSync::TreeAccessRequest::TreeAccessRequest(int driveId, const FsPath& deviceDirectory, DoForAllDirContentFuctionObj& funcobj, RequestCallback* callback) :
	_drv(driveId), 
	_dir(deviceDirectory), 
	_dir2(L"unused"),
	_dirfunc(&funcobj),
    _callback(callback),
	_type(INTERNAL_DISCOVER_DIRECTORY)
{ 

}

ConnectionSync::TreeAccessRequest::TreeAccessRequest(int /*unused*/, int driveId, const FsPath& deviceInode, DoForAllDirContentFuctionObj& funcobj, RequestCallback* callback) :
	_drv(driveId),
	_dir(deviceInode),
	_dir2(L"unused"),
	_dirfunc(&funcobj),
	_callback(callback),
	_type(INTERNAL_DISCOVER_INODE)
{

}

ConnectionSync::TreeAccessRequest::TreeAccessRequest(int driveId, const FsPath& deviceInode, const std::wstring& newName, RequestCallback* callback) :
	_drv(driveId),
	_dir(deviceInode),
	_dir2(newName),
	_dirfunc(NULL),
	_callback(callback),
	_type(INTERNAL_RENAME_INODE)
{

}

ConnectionSync::TreeAccessRequest::TreeAccessRequest(int driveId, const FsPath& deviceInode, FsPath& newDeviceInode, RequestCallback* callback) :
	_drv(driveId),
	_dir(deviceInode),
	_dir2(newDeviceInode.getFullPath()),
	_dirfunc(NULL),
	_callback(callback),
	_type(INTERNAL_MOVE_INODE)
{

}

ConnectionSync::TreeAccessRequest::TreeAccessRequest(int driveId, const FsPath& deviceDirectory, UserTreeAccessCmdType type, RequestCallback* callback) :
	_drv(driveId),
	_dir(deviceDirectory),
	_dir2(L"unused"),
	_dirfunc(NULL),
	_callback(callback)
{
	switch (type)
	{
	case CREATE_AS_FILE: _type = INTERNAL_CREATE_AS_FILE; break;
	case CREATE_AS_DIRECTORY: _type =INTERNAL_CREATE_AS_DIRECTORY; break;
	case DELETE_DIR_OR_FILE: _type = INTERNAL_DELETE_DIR_OR_FILE; break;
	case CHECK_DIR_EXISTANCE: _type = INTERNAL_DIR_EXISTANCE; break;
	case CHECK_FILE_EXISTANCE: _type = INTERNAL_FILE_EXISTANCE; break;
	}
}

void ConnectionSync::enqueueFilePushRequest(const FilePushRequest& request)
{
	_filePushRequestsMutex->lock();
	_filePushRequests.push(request);
	_filePushRequestsMutex->unlock();
	_threadWakeup();
}

bool ConnectionSync::doFilePushRequest(FilePushRequest& request)
{
	WaitOnResult w;
	request._callback = &w;
	enqueueFilePushRequest(request);
	return w.wait();
}

void ConnectionSync::enqueueFilePullRequest(const FilePullRequest& request)
{
	_filePullRequestsMutex->lock();
	_filePullRequests.push(request);
	_filePullRequestsMutex->unlock();
	_threadWakeup();
}

bool ConnectionSync::doFilePullRequest(FilePullRequest& request)
{
	WaitOnResult w;
	request._callback = &w;
	enqueueFilePullRequest(request);
	return w.wait();
}

void ConnectionSync::enqueueTreeAccessRequest(const TreeAccessRequest& request)
{
	_treeAccessRequestsMutex->lock();
	_treeAccessRequests.push(request);
	_treeAccessRequestsMutex->unlock();
	_threadWakeup();
}

bool ConnectionSync::doTreeAccessRequest(TreeAccessRequest& request)
{
	WaitOnResult w;
	request._callback = &w;
	enqueueTreeAccessRequest(request);
	return w.wait();
}

void ConnectionSyncThreadRoutine( ConnectionSync* c)
{
	while (1)
	{
		if (c->_threadHandleTreeAccessReq()) { continue; }
		if (c->_threadHandleFilePullReq()) { continue; }
		if (!c->_threadHandleFilePushReq()) 
		{ 
			if (c->_threadNormalStop) { break; }
			c->_threadYield(); 
		}
	}
}

bool ConnectionSync::_threadHandleTreeAccessReq()
{
	_treeAccessRequestsMutex->lock();
	if (_treeAccessRequests.empty())
	{
		_treeAccessRequestsMutex->unlock();
		return false;
	}
	else
	{
		TreeAccessRequest req = _treeAccessRequests.front();
		_treeAccessRequests.pop();
		_treeAccessRequestsMutex->unlock();

		if (_outputter != NULL)
		{
			std::wstring filenameForOutput = req._dir.getFullPath();
			_outputterLock.lock();
			if (_outputter != NULL) { (*_outputter) << "Accessing file " << Utils::wstringToString(filenameForOutput) << std::endl; }
			_outputterLock.unlock();
		}

		AbstractMappableDrive* drv = NULL;
		try
		{
			drv = _myDrives.at(req._drv);
		}
		catch (std::out_of_range)
		{
			if (req._callback != NULL) { req._callback->callback(false); }
			return true;
		}


		//Multiple actions possible:
		switch (req._type)
		{
			case ConnectionSync::INTERNAL_DISCOVER_DIRECTORY :
			{
				if (req._dirfunc == NULL || drv->checkIfExists(req._dir) != AbstractMappableDrive::IS_DIRECTORY)
				{
					if (req._callback != NULL) { req._callback->callback(false); }
					return true;
				}
				bool ok = drv->doForAllDirContent(req._dir, *(req._dirfunc));
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
			case ConnectionSync::INTERNAL_DELETE_DIR_OR_FILE:
			{
				if (AbstractMappableDrive::DOES_NOT_EXIST == drv->checkIfExists(req._dir))
				{
					if (req._callback != NULL) { req._callback->callback(true); }
					return true;
				}
				bool ok = drv->deleteFileOrDirectory(req._dir, false);
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
			case ConnectionSync::INTERNAL_CREATE_AS_FILE :
			{
				if (req._dir.getPathLength() < 1) { if (req._callback != NULL) { req._callback->callback(false); } return true; } //path too short
				AbstractMappableDrive::checkIfExistsReturn r = drv->checkIfExists(req._dir);
				if (r == AbstractMappableDrive::IS_FILE) { if (req._callback != NULL) { req._callback->callback(true); return true; } } //file already exists
				if (r == AbstractMappableDrive::IS_DIRECTORY) { if (req._callback != NULL) { req._callback->callback(false); return true; } } //name collision with directory
				
				FsPath baseDir = req._dir.getLeftSubPath(req._dir.getPathLength() - 2);
				std::wstring fileName = req._dir.getPathOnLayer(req._dir.getPathLength() - 1).getActualCase();

				std::string pseudoFileContent("");
				std::stringstream pseudoFile(pseudoFileContent);
				bool ok = drv->createFileWithStream(baseDir, fileName, pseudoFile, pseudoFileContent.size());
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
			case ConnectionSync::INTERNAL_CREATE_AS_DIRECTORY :
			{
				if (req._dir.getPathLength() < 1) { if (req._callback != NULL) { req._callback->callback(false); } return true; }
				AbstractMappableDrive::checkIfExistsReturn r = drv->checkIfExists(req._dir);
				if (r == AbstractMappableDrive::IS_FILE) { if (req._callback != NULL) { req._callback->callback(false); return true; } } //name collision with file
				if (r == AbstractMappableDrive::IS_DIRECTORY) { if (req._callback != NULL) { req._callback->callback(true); return true; } } //directory already exists

				FsPath baseDir = req._dir.getLeftSubPath(req._dir.getPathLength() - 2);
				std::wstring dirName = req._dir.getPathOnLayer(req._dir.getPathLength() - 1).getActualCase();

				bool ok = drv->createFolder(baseDir, dirName);
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
			case ConnectionSync::INTERNAL_DIR_EXISTANCE:
			{
				if (req._callback != NULL)
				{
					req._callback->callback((AbstractMappableDrive::IS_DIRECTORY == drv->checkIfExists(req._dir)) ? true : false);
				}
				break;
			}
			case ConnectionSync::INTERNAL_FILE_EXISTANCE:
			{
				if (req._callback != NULL)
				{
					req._callback->callback( (AbstractMappableDrive::IS_FILE == drv->checkIfExists(req._dir)) ? true : false );
				}
				break;
			}
			case ConnectionSync::INTERNAL_DISCOVER_INODE :
			{
				if (req._dirfunc == NULL )
				{
					if (req._callback != NULL) { req._callback->callback(false); }
					return true;
				}
				bool ok = drv->doForSingleObject(req._dir, *(req._dirfunc));
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
			case ConnectionSync::INTERNAL_RENAME_INODE :
			{
				bool ok = drv->renameObject(req._dir, CaseInsensitiveWstring(req._dir2));
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
			case ConnectionSync::INTERNAL_MOVE_INODE :
			{
				bool ok = drv->moveNode(req._dir, FsPath(req._dir2));
				if (req._callback != NULL) { req._callback->callback(ok); }
				break;
			}
		}
	}
	return true;
}

bool ConnectionSync::_threadHandleFilePullReq()
{
	_filePullRequestsMutex->lock();
	if (_filePullRequests.empty())
	{
		_filePullRequestsMutex->unlock();
		return false;
	}
	else
	{
		FilePullRequest req = _filePullRequests.front();
		_filePullRequests.pop();
		_filePullRequestsMutex->unlock();

		if (_outputter != NULL)
		{
			std::wstring filenameForOutput = req._dFile.getFullPath();
			_outputterLock.lock();
			if (_outputter != NULL) { (*_outputter) << "Pulling file " << Utils::wstringToString(filenameForOutput) << std::endl; }
			_outputterLock.unlock();
		}

		AbstractMappableDrive* drv;
		try
		{
			drv = _myDrives.at(req._drv);
		}
		catch (std::out_of_range)
		{
			if (req._callback != NULL) { req._callback->callback(false); }
			return true;
		}

		if (drv->checkIfExists(req._dFile) != AbstractMappableDrive::IS_FILE)
		{
			if (req._callback != NULL) { req._callback->callback(false); }
			return true;
		}

		std::ofstream cachefile(req._pFile, std::ios::binary);
		bool ok = drv->readFile(req._dFile, cachefile);
		cachefile.close();
		if (req._callback != NULL) { req._callback->callback(ok); }
	}
	return true;
}

bool ConnectionSync::_threadHandleFilePushReq()
{
	_filePushRequestsMutex->lock();
	if (_filePushRequests.empty())
	{
		_filePushRequestsMutex->unlock();
		return false;
	}
	else
	{
		FilePushRequest req = _filePushRequests.front();
		_filePushRequests.pop();
		_filePushRequestsMutex->unlock();

		if (_outputter != NULL)
		{
			std::wstring filenameForOutput = req._dFile.getFullPath();
			_outputterLock.lock();
			if (_outputter != NULL) { (*_outputter) << "Pushing file " << Utils::wstringToString(filenameForOutput) << std::endl; }
			_outputterLock.unlock();
		}

		AbstractMappableDrive* drv;
		try
		{
			drv = _myDrives.at(req._drv);
		}
		catch (std::out_of_range)
		{
			if (req._callback != NULL) { req._callback->callback(false); }
			return true;
		}

		unsigned long long cachefilesize = Utils::getOSFileSize(req._pFile);
		std::ifstream cachefile(req._pFile, std::ios::binary);
		if (cachefilesize == 0 || !cachefile.good())
		{
			if (req._callback != NULL) { req._callback->callback(false); }
		}
		else
		{
			bool ok = drv->writeFile(req._dFile, cachefile, cachefilesize);
			if (req._callback != NULL) { req._callback->callback(ok); }
		}
	}
	return true;
}

ConnectionSync::~ConnectionSync()
{
	_threadNormalStop = true;
	_threadWakeup();
	myThread->join();
	_outputter = NULL;
	delete myThread;
	delete _filePullRequestsMutex;
	delete _filePushRequestsMutex;
	delete _treeAccessRequestsMutex;
}
