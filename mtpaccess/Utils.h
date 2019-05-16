#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <atomic>
#include <Windows.h>

#define DEPRECATED 

#define ActuallyDeleteComObject( comobjptr ) while( comobjptr->Release() > 0 ) { }

class Utils
{
public:
	enum FileType
	{
		FILETYPE_UNKNOWN = 0,
		FILETYPE_MUSIC = 1,
		FILETYPE_IMAGE = 2,
		FILETYPE_CONTACT = 3
	};

	//Converts a std::wstring to a std::string.
	//Parameters:
	//(1) IN	std::wstring& string_to_convert		The widestring you whish to convert. Remains unchanged.
	//Returns: The resulting std::string
	static std::string wstringToString(std::wstring& string_to_convert);

	static std::wstring stringToWstring(std::string& string_to_convert);

	//Extracts the file ending out of a file name
	//Parameters:
	//(1) IN	std::wstring& filename	the filename of which you want to get the file ending. Remains unchanged.
	//Returns: The file ending, including the dot (e.g. "test.abc" -> ".abc" )
	static std::wstring getFileEnding(std::wstring& filename);

	//Extracts the friendly file name (without ending) out of a file name
	//Parameters:
	//(1) IN	std::wstring& filename	the filename of which you want to get the friendly version. Remains unchanged.
	//Returns: The friendly file name, excluding the last dot (e.g. "test.abc" -> "test" )
	static std::wstring getFileNameWithoutEnding(std::wstring& filename);

	//Classify a file by its file ending
	//Parameters:
	//(1) IN	std::wstring& filename	the filename of the file to classify. This file doesn't have to exist on any file system. Remains Unchanged.
	//Returns: Any of Utils::FileName
	static FileType classifyFileByName(std::wstring& filename);

	//Get the size of a file present in a file system reachable by the C(++) Standard Library <fstream> - does not work for MTP File Systems
	//Parameters:
	//(1) IN	std::wstring& filename	filename in <fstream> friendly format of the file which you want to get the size from.
	//Returns: Size of this file in byte.
	static unsigned long long getOSFileSize(const std::wstring& filename);

	static size_t tokenize(std::wstring in, std::vector<std::wstring>& out);

	static size_t tokenize(std::string in, std::vector<std::string>& out);

	static bool deleteOSfile(std::wstring filename);

	static bool binaryCompareFiles(const std::wstring& file1, const std::wstring& file2);

	//static int getNewHash(const std::wstring& seed);

	static void splitLongInteger(const unsigned long long input, int& low, int& high);

	static std::string getAvailableDrives();

	static bool isDirectoryEmpty(std::wstring osDir, std::unordered_set<std::wstring> except = {});

	static bool checkDirectoryExistance(std::wstring osDir);

	static bool createOSDirectory(std::wstring path);

	static bool deleteAllOSDirCont(std::wstring osDir, std::unordered_set<std::wstring> except = {});

	static std::wstring getExecutablePath();

	static FILETIME timeConvert(ULONGLONG deviceTime);
};

class CaseInsensitiveWstring;

class FsPath
{
private:
	std::vector<CaseInsensitiveWstring> _path;
public:
	FsPath(std::wstring str);
	int getPathLength() { return static_cast<int>( _path.size() ); }
	CaseInsensitiveWstring getPathOnLayer(int layer);
	FsPath getLeftSubPath(int until);
	FsPath getRightSubPath(int from);
	std::wstring getFullPath();
	friend bool operator<(const FsPath& rhs, const FsPath& lhs);
};

bool operator<(const FsPath& rhs, const FsPath& lhs);

template<typename T>
class AutoDeletePointer
{
private:
	T* toDelete;
public:
	AutoDeletePointer(T* onThisObject) : toDelete(onThisObject) { }
	~AutoDeletePointer() { delete toDelete; }
};

class CaseInsensitiveWstring
{
private:
	std::wstring actualCase;
	std::wstring upperCase;
public:
	CaseInsensitiveWstring(const std::wstring& str): actualCase(str), upperCase(str) { std::transform(upperCase.begin(), upperCase.end(), upperCase.begin(), ::towupper); }
	CaseInsensitiveWstring& operator=(const std::wstring& str) {
		actualCase = upperCase = str;
		std::transform(upperCase.begin(), upperCase.end(), upperCase.begin(), ::towupper);
		return *this;
	}
	std::wstring getActualCase() { return actualCase; }
	std::wstring getUpperCase() { return upperCase; }
	bool operator==(const CaseInsensitiveWstring& lhs) const { return this->upperCase == lhs.upperCase; }
	bool operator<(const CaseInsensitiveWstring& lhs) const { return this->upperCase < lhs.upperCase; }
	bool operator>(const CaseInsensitiveWstring& lhs) const { return this->upperCase > lhs.upperCase; }
	bool operator<=(const CaseInsensitiveWstring& lhs) const { return this->upperCase <= lhs.upperCase; }
	bool operator>=(const CaseInsensitiveWstring& lhs) const { return this->upperCase >= lhs.upperCase; }
	bool operator!=(const CaseInsensitiveWstring& lhs) const { return this->upperCase != lhs.upperCase; }
	//friend bool operator!=(const CaseInsensitiveWstring& rhs, const CaseInsensitiveWstring lhs);
	//friend bool operator<()
};
/*
bool operator!=(const CaseInsensitiveWstring& rhs, const CaseInsensitiveWstring lhs)
{
	return rhs != lhs;
}

bool operator<(const CaseInsensitiveWstring& rhs, const CaseInsensitiveWstring lhs)
{
	return rhs < lhs;
}*/


class ConflictlessHashProvider
{
private:
	std::unordered_set<size_t> _alreadyUsedHashes;
	int _conflictTracker;
	std::hash<std::wstring> _generator;
public:
	ConflictlessHashProvider() : _conflictTracker(0) { }
	std::wstring getHash(const std::wstring& seed);
	int dbgGetConflicts() { return _conflictTracker; }
};

class ParalellReadUniqueWriteSync
{
private:
	int _readAccesses;
	bool _writeAccess;
	bool _writeRequest;
	std::condition_variable readEndEvent;
	std::condition_variable writeEndEvent;
	std::mutex _writeMtx;
	std::mutex _statusMtx;
public:
	ParalellReadUniqueWriteSync() : _readAccesses(0), _writeAccess(false), _writeRequest(false) { }
	void getReadAccess();
	void endReadAccess();
	void getWriteAccess();
	void endWriteAccess();
};

class CacheLocationProvider
{
private:
	static CacheLocationProvider* _instance;
	CacheLocationProvider();
	std::wstring _cacheLocation;
	static std::wstring getAppdataPath();
	static std::wstring getRedirectedPath();
	static bool makeItMine(std::wstring dir);
	static bool isMine(std::wstring dir);
public:
	static CacheLocationProvider& getInstance();
	std::wstring getCacheLocation() { return _cacheLocation; }
	std::wstring getIntermediateStore() { return _cacheLocation + L"\\intermediatestore"; }
};