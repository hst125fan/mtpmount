
#include "MemLeakDetection.h"


#include "DokanDriveWrapper.h"
#include <iostream>
#include <io.h>

bool isThisAWriteAccess(ACCESS_MASK DesiredAccess)
{
	if (DesiredAccess & GENERIC_ALL || DesiredAccess & GENERIC_WRITE) { return true; }
	if (DesiredAccess & FILE_WRITE_DATA || DesiredAccess & FILE_APPEND_DATA) { return true; } 
	if (DesiredAccess & FILE_WRITE_ATTRIBUTES || DesiredAccess & FILE_WRITE_EA) { return true; }
	return false;
}

DokanDriveWrapper::DokanDriveWrapper(ConnectionSync& connsyncer, FileCache& fcache,int driveID, char driveletter, const std::wstring& volumename) : _myConnection(&connsyncer), _myCache(&fcache), _driveId(driveID), _mountstatus(STATUS_UNKNOWN), _volumename(volumename)
{
	memset(&dokanoptions, 0, sizeof(DOKAN_OPTIONS));
	dokanoptions.GlobalContext = (ULONG64)this;
	_dokanmntpnt.append(1, driveletter);
	_dokanmntpnt.append(L":\\");
	dokanoptions.MountPoint = _dokanmntpnt.c_str();
	dokanoptions.Options = DOKAN_OPTION_CURRENT_SESSION | DOKAN_OPTION_REMOVABLE;
	dokanoptions.Version = 120;

	memset(&dokanoperations, 0, sizeof(DOKAN_OPERATIONS));
	dokanoperations.Mounted = DokanMounted;
	dokanoperations.ZwCreateFile = DokanCreateFile;
	dokanoperations.Cleanup = DokanCleanup;
	dokanoperations.CloseFile = DokanCloseFile;
	dokanoperations.FindFiles = DokanFindFiles;
	dokanoperations.DeleteDirectory = DokanDeleteDirectory;
	dokanoperations.DeleteFileA = DokanDeleteFile;
	dokanoperations.ReadFile = DokanReadFile;
	dokanoperations.WriteFile = DokanWriteFile;
	dokanoperations.GetVolumeInformationA = DokanVolumeInfo;
	dokanoperations.GetFileInformation = DokanObjectInfo;
	dokanoperations.SetAllocationSize = DokanSetAllocationSize;
	dokanoperations.SetEndOfFile = DokanSetEndOfFile;
	dokanoperations.SetFileSecurityA = DokanSetFileSecurity;
	dokanoperations.GetFileSecurityA = DokanGetFileSecurity;
	dokanoperations.MoveFileA = DokanMoveFile;


	_myThread = new std::thread(DokanDriveWrapperThreadRoutine, this);

	std::mutex waitmtx;
	std::unique_lock<std::mutex> waitlck(waitmtx);
	_mountstatusMutex.lock();
	while (_mountstatus == STATUS_UNKNOWN)
	{
		_mountstatusMutex.unlock();
		_mountedCallback.wait(waitlck);
		_mountstatusMutex.lock();
	}
	if (_mountstatus == STATUS_UNMOUNTED)
	{
		_mountstatusMutex.unlock();
		throw DokanDriveWrapperMountFailedException(_dokanreturn);
	}
	_mountstatusMutex.unlock();
}

DokanDriveWrapper::~DokanDriveWrapper()
{
	try
	{
		DokanUnmount(getDriveLetter());
	}
	catch (DokanDriveWrapperNotMountedException)
	{
		return;
	}

	std::mutex waitmtx;
	std::unique_lock<std::mutex> waitlck(waitmtx);
	_mountstatusMutex.lock();
	while (_mountstatus == STATUS_MOUNTED)
	{
		_mountstatusMutex.unlock();
		_mountedCallback.wait(waitlck);
		_mountstatusMutex.lock();
	}
	_mountstatusMutex.unlock();
	_myThread->join();
	delete _myThread;
}

char DokanDriveWrapper::getDriveLetter()
{
	if (_mountstatus == STATUS_MOUNTED)
	{
		return static_cast<char>( dokanoptions.MountPoint[0] );
	}
	throw DokanDriveWrapperNotMountedException();
}

bool DokanDriveWrapper::_checkDirExistance(const FsPath& dirpath)
{
	ConnectionSync::TreeAccessRequest req(_driveId, dirpath, ConnectionSync::CHECK_DIR_EXISTANCE, NULL);
	return _myConnection->doTreeAccessRequest(req);
}

bool DokanDriveWrapper::_checkFileExistance(const FsPath& filepath)
{
	ConnectionSync::TreeAccessRequest req(_driveId, filepath, ConnectionSync::CHECK_FILE_EXISTANCE, NULL);
	return _myConnection->doTreeAccessRequest(req);
}

void DokanDriveWrapper::_deleteNode(const FsPath& nodepath)
{
	ConnectionSync::TreeAccessRequest req(_driveId, nodepath, ConnectionSync::DELETE_DIR_OR_FILE, NULL);
	_myConnection->doTreeAccessRequest(req);
}

bool DokanDriveWrapper::_createNewFile(const FsPath& filepath)
{
	ConnectionSync::TreeAccessRequest req(_driveId, filepath, ConnectionSync::CREATE_AS_FILE, NULL);
	return _myConnection->doTreeAccessRequest(req);
}

bool DokanDriveWrapper::_createNewDir(const FsPath& dirpath)
{
	ConnectionSync::TreeAccessRequest req(_driveId, dirpath, ConnectionSync::CREATE_AS_DIRECTORY, NULL);
	return _myConnection->doTreeAccessRequest(req);
}

bool DokanDriveWrapper::_isDirEmpty(const FsPath& dirpath)
{
	CountDirectoryContents counter;
	ConnectionSync::TreeAccessRequest req(_driveId, dirpath, counter, NULL);
	_myConnection->doTreeAccessRequest(req);
	return (counter.getCount() == 0) ? true : false;
}

bool ForwardToDokanFillFindData::doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
{
	PWIN32_FIND_DATAW finddata = new WIN32_FIND_DATAW;
	memset(finddata, 0, sizeof(WIN32_FIND_DATAW));
	lstrcpynW(finddata->cFileName, currentNode->getNodeName().c_str(), 260);

	if (currentNode->isDirectory())
	{
		finddata->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
	}

	//Timestamps optional, only modification time used!
	finddata->ftLastWriteTime = currentNode->getModificationTime();
	finddata->ftCreationTime = finddata->ftLastWriteTime;
	finddata->ftLastAccessTime = finddata->ftLastWriteTime;

	unsigned long long fileSize = c->getFileSizeOfCachedFile(currentNode->getNodePath(), _drvId);
	if (fileSize == 0) { fileSize = currentNode->getPayloadSize(); }
	int fileSizeLower, fileSizeUpper;
	Utils::splitLongInteger(fileSize, fileSizeLower, fileSizeUpper);
	finddata->nFileSizeLow = fileSizeLower;
	finddata->nFileSizeHigh = fileSizeUpper;

	_forward(finddata, _finfo);
	delete finddata;
	return true;
}

bool ForwardToDokanFillFindDataPattern::doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
{
	if (!DokanIsNameInExpression(_pattern, currentNode->getNodeName().c_str(), TRUE))
	{
		return true;
	}
	return _dataFiller.doForAllDirContentMethod(currentNode);
}

void DokanDriveWrapperThreadRoutine(DokanDriveWrapper* d)
{
	d->_dokanreturn = DokanMain(&(d->dokanoptions), &(d->dokanoperations));
	d->_mountstatusMutex.lock();
	d->_mountstatus = DokanDriveWrapper::STATUS_UNMOUNTED;
	d->_mountedCallback.notify_all();
	d->_mountstatusMutex.unlock();
}


NTSTATUS DOKAN_CALLBACK DokanMounted(PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	d->_mountstatusMutex.lock();
	d->_mountstatus = DokanDriveWrapper::STATUS_MOUNTED;
	d->_mountedCallback.notify_all();
	d->_mountstatusMutex.unlock();
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);
	DokanFileInfo->Context = NULL;

	if (d->_checkDirExistance(path))
	{
		//open directory
		DokanFileInfo->IsDirectory = true;
		if (CreateDisposition == CREATE_NEW) { return STATUS_OBJECT_NAME_EXISTS; }
		if (CreateDisposition == OPEN_ALWAYS || CreateDisposition == CREATE_ALWAYS) { return STATUS_OBJECT_NAME_COLLISION; }

		//DokanFileInfo->Context = d->_obtainDirOverhop(isThisAWriteAccess(DesiredAccess));
		return STATUS_SUCCESS;
	}
	else if (d->_checkFileExistance(path))
	{
		//open file
		if (DokanFileInfo->IsDirectory) { return STATUS_NOT_A_DIRECTORY; }

		//DokanFileInfo->Context = d->_obtainFileOverhop(isThisAWriteAccess(DesiredAccess), L"UNUSED");
		if (CreateDisposition == CREATE_NEW) { return STATUS_OBJECT_NAME_EXISTS; }
		if (CreateDisposition == FILE_SUPERSEDE || CreateDisposition == FILE_OVERWRITE || CreateDisposition == FILE_OVERWRITE_IF)
		{
			std::wstring clearfilename = c->beginAccessByFileName(path, d->_driveId, true);
			std::ofstream fileclearer(clearfilename, std::ios::trunc);
			c->endFileAccessByName(path, d->_driveId, true);
		}
		return (CreateDisposition == OPEN_ALWAYS || CreateDisposition == CREATE_ALWAYS) ? STATUS_OBJECT_NAME_COLLISION : STATUS_SUCCESS;
	}
	else if(DokanFileInfo->IsDirectory)
	{
		//create directory
		if (CreateDisposition == FILE_OPEN || CreateDisposition == FILE_OVERWRITE) { return STATUS_OBJECT_NAME_NOT_FOUND; }
		if (!d->_createNewDir(path)) { return STATUS_ACCESS_DENIED; }

		//DokanFileInfo->Context = d->_obtainDirOverhop(isThisAWriteAccess(DesiredAccess));
		return STATUS_SUCCESS;
	}
	else
	{
		//create file
		if (CreateDisposition == FILE_OPEN || CreateDisposition == FILE_OVERWRITE) { return STATUS_OBJECT_NAME_NOT_FOUND; }
		if (!d->_createNewFile(path)) { return STATUS_ACCESS_DENIED; }

		//DokanFileInfo->Context = d->_obtainFileOverhop(isThisAWriteAccess(DesiredAccess), L"UNUSED");
		return STATUS_SUCCESS;
	}
}

void DOKAN_CALLBACK DokanCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);

	if (DokanFileInfo->DeleteOnClose)
	{
		c->deleteCacheEntry(path, d->_driveId);
		d->_deleteNode(path);
	}
}

void DOKAN_CALLBACK DokanCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
}

NTSTATUS DOKAN_CALLBACK DokanFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);

	if (!d->_checkDirExistance(path))
	{
		return STATUS_OBJECT_PATH_NOT_FOUND;
	}

	ForwardToDokanFillFindData discovery(FillFindData, DokanFileInfo, *c, d->_driveId);
	ConnectionSync::TreeAccessRequest req(d->_driveId, path, discovery, NULL);

	if (!s->doTreeAccessRequest(req))
	{
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanFindFilesPattern(LPCWSTR FileName, LPCWSTR SearchPattern, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);

	if (!d->_checkDirExistance(path))
	{
		return STATUS_OBJECT_PATH_NOT_FOUND;
	}

	ForwardToDokanFillFindDataPattern discovery(FillFindData, DokanFileInfo, *c, d->_driveId, SearchPattern);
	ConnectionSync::TreeAccessRequest req(d->_driveId, path, discovery, NULL);

	if (!s->doTreeAccessRequest(req))
	{
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	if (DokanFileInfo->IsDirectory) { return STATUS_UNSUCCESSFUL; }
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);
	return (c->pushSingleFile(path, d->_driveId)) ? STATUS_SUCCESS : STATUS_OBJECT_PATH_NOT_FOUND;
}

NTSTATUS DOKAN_CALLBACK DokanDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	if (DokanFileInfo->DeleteOnClose == FALSE) { return STATUS_SUCCESS; }
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);

	if (!d->_checkDirExistance(path)) { return STATUS_OBJECT_PATH_NOT_FOUND; }
	if (!d->_isDirEmpty(path)) { return STATUS_DIRECTORY_NOT_EMPTY; }
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo)
{
	if (DokanFileInfo->DeleteOnClose == FALSE) { return STATUS_SUCCESS; }
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);

	if (!d->_checkFileExistance(path)) { return STATUS_OBJECT_PATH_NOT_FOUND; }
	//if(!c->getIdleStatus(path,d->_driveId)) { return STATUS_ACCESS_DENIED;}
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;

	std::wstring cachefilename = c->beginAccessByFileName(FsPath(FileName), d->_driveId, false);

	std::ifstream stream(cachefilename, std::ios::binary);
	stream.seekg(Offset);
	char* buf = (char*)Buffer;
	DWORD i = 0;
	while(i < BufferLength && stream.good())
	{
		
		buf[i] = stream.get();
		if (stream.eof()) { break; }
		i++;
	}
	*ReadLength = i;
	c->endFileAccessByName(FsPath(FileName), d->_driveId, false);
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;

	std::wstring cachefilename = c->beginAccessByFileName(FsPath(FileName), d->_driveId, true);

	std::fstream stream(cachefilename, std::ios::binary | std::ios::ate | std::ios::in | std::ios::out);
	stream.seekp(Offset);
	char* buf = (char*)Buffer;
	DWORD i = 0;
	while (i < NumberOfBytesToWrite && stream.good())
	{
		stream.put(buf[i]);
		i++;
	}
	*NumberOfBytesWritten = i;
	c->endFileAccessByName(FsPath(FileName), d->_driveId, true);
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanVolumeInfo(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	lstrcpynW(VolumeNameBuffer, d->_volumename.c_str(), VolumeNameSize);
	*VolumeSerialNumber = 2804; //This is not true - change it!
	*MaximumComponentLength = 200;
	*FileSystemFlags = FILE_UNICODE_ON_DISK;
	lstrcpynW(FileSystemNameBuffer, L"NTFS\0", FileSystemNameSize);
	return STATUS_SUCCESS;
}

NTSTATUS DOKAN_CALLBACK DokanGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS DOKAN_CALLBACK DokanSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS DOKAN_CALLBACK DokanSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;

	std::wstring cachefilename = c->beginAccessByFileName(FsPath(FileName), d->_driveId, true);

	FILE* pFile = NULL;
	_wfopen_s(&pFile, cachefilename.c_str(), L"a+b");
	if (pFile == NULL) { c->endFileAccessByName(FsPath(FileName), d->_driveId, true);  return STATUS_UNSUCCESSFUL; }
	int ret = _chsize_s(_fileno(pFile), AllocSize);
	fclose(pFile);
	c->endFileAccessByName(FsPath(FileName), d->_driveId, true);
	return (ret == 0) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
}

NTSTATUS DOKAN_CALLBACK DokanSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;

	std::wstring cachefilename = c->beginAccessByFileName(FsPath(FileName), d->_driveId, true);

	FILE* pFile = NULL;
	_wfopen_s(&pFile, cachefilename.c_str(), L"a+b");
	if (pFile == NULL) { c->endFileAccessByName(FsPath(FileName), d->_driveId, true); return STATUS_UNSUCCESSFUL; }
	int ret = _chsize_s(_fileno(pFile), ByteOffset);
	fclose(pFile);
	c->endFileAccessByName(FsPath(FileName), d->_driveId, true);
	return (ret == 0) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
}

NTSTATUS DOKAN_CALLBACK DokanMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath oldPath(FileName);
	FsPath newPath(NewFileName);

	if (d->_checkFileExistance(newPath))
	{
		if (!d->_checkFileExistance(oldPath)) { return STATUS_UNSUCCESSFUL; }
		if (!ReplaceIfExisting) { return STATUS_UNSUCCESSFUL; }
		d->_deleteNode(newPath);
	}
	else if (d->_checkDirExistance(newPath))
	{
		if (!d->_checkDirExistance(oldPath)) { return STATUS_UNSUCCESSFUL; }
		return STATUS_OBJECT_NAME_COLLISION;
	}

	bool success = c->moveOrRenameInode(oldPath, newPath, d->_driveId);
	return (success) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS DOKAN_CALLBACK DokanObjectInfo(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo)
{
	DokanDriveWrapper* d = (DokanDriveWrapper*)DokanFileInfo->DokanOptions->GlobalContext;
	ConnectionSync* s = d->_myConnection;
	FileCache* c = d->_myCache;
	FsPath path(FileName);

	Buffer->nNumberOfLinks = 1; //we don't support hardlinks
	Buffer->nFileIndexHigh = 0; //and I really don't know how to get that
	Buffer->nFileIndexLow = 0;
	Buffer->dwVolumeSerialNumber = 2804; //This is not true - change it!

	unsigned long long fileSize = c->getFileSizeOfCachedFile(FsPath(FileName), d->_driveId);
	int fileSizeLower, fileSizeUpper;
	Utils::splitLongInteger(fileSize, fileSizeLower, fileSizeUpper);
	Buffer->nFileSizeHigh = fileSizeUpper;
	Buffer->nFileSizeLow = fileSizeLower;

	ForwardToObjectInfo fileinfofill(Buffer, DokanFileInfo);
	ConnectionSync::TreeAccessRequest req(1, d->_driveId, path, fileinfofill, NULL);
	if (s->doTreeAccessRequest(req))
	{
		return STATUS_SUCCESS;
	}
	else
	{
		return STATUS_UNSUCCESSFUL;
	}
}

bool ForwardToObjectInfo::doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
{
	//only modification time used!
	_forward->ftLastWriteTime = currentNode->getModificationTime();
	_forward->ftCreationTime = _forward->ftLastWriteTime;
	_forward->ftLastAccessTime = _forward->ftLastWriteTime;
	if (currentNode->isDirectory())
	{
		_forward->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		_forward->nFileSizeHigh = 0;
		_forward->nFileIndexLow = 0;
	}
	else
	{
		_forward->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;

		if (_forward->nFileSizeLow == 0 && _forward->nFileSizeHigh == 0)
		{
			unsigned long long filesize = currentNode->getPayloadSize();
			int filesizeUp, filesizeLow;
			Utils::splitLongInteger(filesize, filesizeLow, filesizeUp);
			_forward->nFileSizeLow = filesizeLow;
			_forward->nFileSizeHigh = filesizeUp;
		}
	}
	return true;
}


ULONG64 DokanDriveWrapper::_obtainFileOverhop(bool write, const std::wstring& cachecopy)
{
	DokanDriveWrapper::FileOverhop* hopper = new DokanDriveWrapper::FileOverhop(write, cachecopy);
	return reinterpret_cast<ULONG64>(hopper);
}


ULONG64 DokanDriveWrapper::_obtainDirOverhop(bool write)
{
	DokanDriveWrapper::DirOverhop* hopper = new DokanDriveWrapper::DirOverhop(write);
	return reinterpret_cast<ULONG64>(hopper);
}

void DokanDriveWrapper::_deleteOverhop(ULONG64 address)
{
	if (address == 0) 
	{ 
		return; 
	}
	DokanDriveWrapper::Overhop* hopper = reinterpret_cast<DokanDriveWrapper::Overhop*>(address);
	delete hopper;
}

bool CopyDirectoryContent::doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
{
	FsPath destPath(_dest.getFullPath() + currentNode->getNodeName());
	FsPath srcPath(currentNode->getNodePath());
	if (currentNode->isDirectory())
	{
		CopyDirectoryContent copycat(destPath, *d);
		ConnectionSync* s = d->_myConnection;
		ConnectionSync::TreeAccessRequest req(d->_driveId, srcPath, copycat, NULL);
		bool error = s->doTreeAccessRequest(req);
		if (!error && !copycat.isFailed()) { return true; }
		else { return false; }
	}
	else
	{
		FileCache* c = d->_myCache;
		bool succeeded = false;
		d->_createNewFile(destPath);
		std::wstring srcInCache = c->beginAccessByFileName(srcPath, d->_driveId, false);
		std::wstring destInCache = c->beginAccessByFileName(destPath, d->_driveId, true);
		if (srcInCache != L"ERROR" && destInCache != L"ERROR")
		{
			succeeded = CopyFileW(srcInCache.c_str(), destInCache.c_str(), FALSE);
		}
		c->endFileAccessByName(srcPath, d->_driveId, false);
		c->endFileAccessByName(srcPath, d->_driveId, true);
		if (succeeded) { return true; }
		else { _error = true; return false; }
	}
}