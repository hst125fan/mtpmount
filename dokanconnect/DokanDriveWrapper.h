#pragma once

#pragma warning ( disable : 4005 )
#include <dokan.h>
#pragma warning ( default : 4005 )
#include <thread>
#include <condition_variable>
#include <exception>
#include "../mtpcache/FileCache.h"

class DokanDriveWrapper;

void DokanDriveWrapperThreadRoutine(DokanDriveWrapper* d);

NTSTATUS DOKAN_CALLBACK DokanMounted(PDOKAN_FILE_INFO fileinfo);
NTSTATUS DOKAN_CALLBACK DokanCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo);
void DOKAN_CALLBACK DokanCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo);
void DOKAN_CALLBACK DokanCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanVolumeInfo(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanFindFilesPattern(LPCWSTR FileName, LPCWSTR SearchPattern, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanObjectInfo(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo);
NTSTATUS DOKAN_CALLBACK DokanSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo);

class DokanDriveWrapperException : public std::exception
{
private:
	std::string msg;
public:
	DokanDriveWrapperException(const std::string& message) : msg(message) { }
	const char* what() { return std::string("DokanDriveWrapperException: " + msg).c_str(); }
};

class DokanDriveWrapperMountFailedException : public DokanDriveWrapperException
{
public:
	DokanDriveWrapperMountFailedException(int dokanreturn) : DokanDriveWrapperException("Mount failed, code=" + dokanreturn) { }
};

class DokanDriveWrapperNotMountedException : public DokanDriveWrapperException
{
public:
	DokanDriveWrapperNotMountedException() : DokanDriveWrapperException("No drive mounted") { }
};

class DokanDriveWrapper
{
private:
	class Overhop
	{
	public:
		virtual ~Overhop() {};
	};
	class FileOverhop : public Overhop
	{
	private:
		bool _write;
		std::wstring _cachecopy;
	public:
		FileOverhop(bool write, const std::wstring& cachecopy) : _write(write), _cachecopy(cachecopy) { }
		bool isWriting() { return _write; }
		std::wstring getCacheCopyFileName() { return _cachecopy; }
	};
	class DirOverhop : public Overhop
	{
	private:
		bool _write;
	public:
		DirOverhop(bool write) : _write(write) { }
	};
	ULONG64 _obtainFileOverhop(bool write, const std::wstring& cachecopy);
	ULONG64 _obtainDirOverhop(bool write);
	void _deleteOverhop(ULONG64 address);
	ConnectionSync* _myConnection;
	FileCache* _myCache;
	int _driveId;
	std::thread* _myThread;

	enum MntSttts
	{
		STATUS_UNKNOWN = 0,
		STATUS_MOUNTED = 1,
		STATUS_UNMOUNTED = 2
	} _mountstatus;
	std::mutex _mountstatusMutex;
	int _dokanreturn;
	std::condition_variable _mountedCallback;

	DOKAN_OPTIONS dokanoptions;
	DOKAN_OPERATIONS dokanoperations;
	std::wstring _volumename;
	std::wstring _dokanmntpnt;

	bool _checkDirExistance(const FsPath& dirpath);
	bool _checkFileExistance(const FsPath& filepath);
	void _deleteNode(const FsPath& nodepath);
	bool _createNewFile(const FsPath& filepath);
	bool _createNewDir(const FsPath& dirpath);
	bool _isDirEmpty(const FsPath& dirpath);

	friend void DokanDriveWrapperThreadRoutine(DokanDriveWrapper* d);
	friend NTSTATUS DOKAN_CALLBACK DokanMounted(PDOKAN_FILE_INFO fileinfo);
	friend NTSTATUS DOKAN_CALLBACK DokanCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo);
	friend void DOKAN_CALLBACK DokanCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo);
	friend void DOKAN_CALLBACK DokanCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanVolumeInfo(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanFindFilesPattern(LPCWSTR FileName, LPCWSTR SearchPattern, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanObjectInfo(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo);
	friend NTSTATUS DOKAN_CALLBACK DokanSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo);
	friend class CopyDirectoryContent;
	friend class ForwardToObjectInfo;
public:
	DokanDriveWrapper(ConnectionSync& connsyncer, FileCache& fcache, int driveID, char driveletter, const std::wstring& volumename);
	~DokanDriveWrapper();
	char getDriveLetter();
};

class CountDirectoryContents : public DoForAllDirContentFuctionObj
{
private:
	int _count;
public:
	CountDirectoryContents() { _count = 0; }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode) { _count++; return true; }
	int getCount() { return _count; }
};

class ForwardToDokanFillFindData : public DoForAllDirContentFuctionObj
{
private:
	PFillFindData _forward;
	PDOKAN_FILE_INFO _finfo;
	FileCache* c;
	int _drvId;
public:
	ForwardToDokanFillFindData(PFillFindData forward, PDOKAN_FILE_INFO fileinfo, FileCache& cache, int drvId) : _forward(forward), _finfo(fileinfo), c(&cache), _drvId(drvId)  { }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode);
};

class ForwardToDokanFillFindDataPattern : public DoForAllDirContentFuctionObj
{
private:
	ForwardToDokanFillFindData _dataFiller;
	LPCWSTR _pattern;
public:
	ForwardToDokanFillFindDataPattern(PFillFindData forward, PDOKAN_FILE_INFO fileinfo, FileCache& cache, int drvId, LPCWSTR pattern) : _dataFiller(forward, fileinfo, cache, drvId), _pattern(pattern) { }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode);
};

class ForwardToObjectInfo : public DoForAllDirContentFuctionObj
{
private:
	LPBY_HANDLE_FILE_INFORMATION _forward;
	PDOKAN_FILE_INFO _finfo;
public:
	ForwardToObjectInfo(LPBY_HANDLE_FILE_INFORMATION forward, PDOKAN_FILE_INFO fileinfo) : _forward(forward), _finfo(fileinfo) { }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode);
};

class CopyDirectoryContent : public DoForAllDirContentFuctionObj
{
private:
	FsPath _dest;
	DokanDriveWrapper* d;
	bool _error;
public:
	CopyDirectoryContent(const FsPath& destinationDir, DokanDriveWrapper& drive) : d(&drive), _dest(destinationDir), _error(false) { }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode);
	bool isFailed() { return _error; }
};




