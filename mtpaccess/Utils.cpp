#include "../dokanconnect/MemLeakDetection.h"
#include "Utils.h"


#include <locale>
#include <codecvt>
#include <Windows.h>
#include <iostream>
#include <unordered_map>
#include <sstream>

FILETIME Utils::timeConvert(ULONGLONG deviceTime)
{
	FILETIME ret;
	ret.dwLowDateTime = static_cast<DWORD>(deviceTime & 0xFFFFFFFFULL);
	ret.dwHighDateTime = static_cast<DWORD>((deviceTime & 0xFFFFFFFF00000000ULL) >> 32);
	return ret;
}

class chs_codecvt : public std::codecvt_byname<wchar_t, char, std::mbstate_t> {
public:
	chs_codecvt() : codecvt_byname("chs") { }
};

std::string Utils::wstringToString(std::wstring& string_to_convert)
{
	//setup converter
	std::wstring_convert<chs_codecvt> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	std::string converted_str = converter.to_bytes(string_to_convert);
	return converted_str;
}

std::wstring Utils::stringToWstring(std::string& string_to_convert)
{
	//setup converter
	std::wstring_convert<chs_codecvt> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	std::wstring converted_str = converter.from_bytes(string_to_convert);
	return converted_str;
}

std::wstring Utils::getFileEnding(std::wstring& filename)
{
	size_t position = filename.find_last_of(L'.');
	if (position == std::wstring::npos)
	{
		return L"";
	}
	else
	{
		return filename.substr(position);
	}
}

std::wstring Utils::getFileNameWithoutEnding(std::wstring& filename)
{
	size_t position = filename.find_last_of(L'.');
	if (position == std::wstring::npos)
	{
		return filename;
	}
	else
	{
		return filename.substr(0, position);
	}
}

unsigned long long Utils::getOSFileSize(const std::wstring& filename)
{
	//There is no call-by-reference, and this is intended - stream should be copied
	std::ifstream s(filename, std::ifstream::binary | std::ifstream::ate);
	return s.tellg();
}

bool Utils::isDirectoryEmpty(std::wstring osDir, std::unordered_set<std::wstring> except)
{
	if (!checkDirectoryExistance(osDir)) { return false; }
	except.insert(L".");
	except.insert(L"..");

	if (osDir.at(osDir.size() - 1) != '\\') { osDir.append(1, '\\'); }
	osDir.append(1, '*');

	std::vector<std::string> allowedDirCont;

	WIN32_FIND_DATAW finddata;
	HANDLE myHandle = FindFirstFileW(osDir.c_str(), &finddata);
	if (myHandle == INVALID_HANDLE_VALUE)
	{
		return (GetLastError() == ERROR_FILE_NOT_FOUND) ? true : false;
	}

	size_t allowancecounter = 0;
	bool ok = true;
	do
	{
		if (except.find(std::wstring(finddata.cFileName)) == except.end())
		{
			ok = false;
			break;
		}
		allowancecounter++;

	} while (FindNextFileW(myHandle, &finddata) && allowancecounter <= except.size());
	FindClose(myHandle);
	return ok && allowancecounter <= except.size();
}

bool Utils::checkDirectoryExistance(std::wstring osDir)
{
	DWORD attribs = GetFileAttributesW(osDir.c_str());
	if (attribs == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		return false;
	}
	return (GetFileAttributesW(osDir.c_str()) & FILE_ATTRIBUTE_DIRECTORY) ? true : false;
}

bool Utils::createOSDirectory(std::wstring path)
{
	if (checkDirectoryExistance(path)) { return true; }
	BOOL ok = CreateDirectoryW(path.c_str(), NULL);
	return ok;
}

bool Utils::deleteAllOSDirCont(std::wstring osDir, std::unordered_set<std::wstring> except)
{
	if (!checkDirectoryExistance(osDir)) { return false; }
	if (isDirectoryEmpty(osDir)) { return true; }

	if (osDir.at(osDir.size() - 1) != '\\') { osDir.append(1, '\\'); }
	std::wstring osDirMask = osDir + L"*";

	WIN32_FIND_DATAW finddata;
	ZeroMemory(&finddata, sizeof(WIN32_FIND_DATAW));
	HANDLE myHandle = FindFirstFileW(osDirMask.c_str(), &finddata);
	
	if (myHandle == INVALID_HANDLE_VALUE)
	{
		return (GetLastError() == ERROR_FILE_NOT_FOUND) ? true : false;
	}

	except.insert(L".");
	except.insert(L"..");

	do {
		if (except.find(std::wstring(finddata.cFileName)) == except.end())
		{
			DeleteFileW(std::wstring(osDir + finddata.cFileName).c_str());
		}
	} while (FindNextFileW(myHandle, &finddata));
	FindClose(myHandle);
	return isDirectoryEmpty(osDir);
}

bool Utils::binaryCompareFiles(const std::wstring& file1, const std::wstring& file2)
{
	if (getOSFileSize(file1) != getOSFileSize(file2))
	{
		return false;
	}

	std::ifstream f1(file1, std::ifstream::binary);
	std::ifstream f2(file2, std::ifstream::binary);
	while (f1.good() && f2.good())
	{
		char f1c = f1.get();
		char f2c = f2.get();
		if (f1c != f2c)
		{
			return false;
		}
	}

	return true;
}

void Utils::splitLongInteger(const unsigned long long input, int& low, int& high)
{
	unsigned long long lowlong =  input & 0x00000000FFFFFFFF;
	unsigned long long highlong = input & 0xFFFFFFFF00000000;
	highlong = highlong >> 32;
	low = static_cast<int>(lowlong);
	high = static_cast<int>(highlong);
}

Utils::FileType Utils::classifyFileByName(std::wstring& filename)
{
	std::wstring ending = getFileEnding(filename);
	if (ending == L"")
	{
		return FILETYPE_UNKNOWN;
	}

	if (ending == L".wav" || ending == L".mp3" || ending == L".aac" || ending == L".m4a")
	{
		return FILETYPE_MUSIC;
	}

	if (ending == L".jpg" || ending == L".jpeg" || ending == L".bmp" || ending == L".png" || ending == L".gif")
	{
		return FILETYPE_IMAGE;
	}

	if (ending == L".vcard")
	{
		return FILETYPE_CONTACT;
	}

	return FILETYPE_UNKNOWN;
}


FsPath::FsPath(std::wstring str)
{
	std::wstring currentToken;
	for (std::wstring::iterator iter = str.begin();
		iter != str.end();
		iter++)
	{
		if (*iter == '\\' || *iter == '/')
		{
			if (!currentToken.empty())
			{
				_path.push_back(currentToken);
				currentToken.clear();
			}
		}
		else
		{
			currentToken.push_back(*iter);
		}
	}
	if (!currentToken.empty())
	{
		_path.push_back(currentToken);
	}
}

CaseInsensitiveWstring FsPath::getPathOnLayer(int layer)
{
	try
	{
		return _path.at(layer);
	}
	catch (std::out_of_range)
	{
		return CaseInsensitiveWstring(L"");
	}
}

FsPath FsPath::getLeftSubPath(int until)
{
	if (until < getPathLength())
	{
		FsPath ret(L"");
		for (int i = 0; i <= until; i++)
		{
			ret._path.push_back(getPathOnLayer(i));
		}
		return ret;
	}
	else
	{
		return *this;
	}
}

FsPath FsPath::getRightSubPath(int from)
{
	if (from < 0)
	{
		return *this;
	}
	else
	{
		FsPath ret(L"");
		for (int i = from; i < getPathLength(); i++)
		{
			ret._path.push_back(getPathOnLayer(i));
		}
		return ret;
	}
}

size_t Utils::tokenize(std::wstring in, std::vector<std::wstring>& out)
{
	out.clear();
	std::wstring currentToken;
	bool escaped = false;
	for (std::wstring::iterator iter = in.begin();
		iter != in.end();
		iter++)
	{
		if ((*iter) == L' ' && !escaped)
		{
			out.push_back(currentToken);
			currentToken.clear();
		}
		else if ((*iter) == L'"')
		{
			escaped = !escaped;
		}
		else
		{
			currentToken.push_back((*iter));
		}
	}
	if (!currentToken.empty())
	{
		out.push_back(currentToken);
	}
	return out.size();
}

size_t Utils::tokenize(std::string in, std::vector<std::string>& out)
{
	out.clear();
	std::string currentToken;
	bool escaped = false;
	for (std::string::iterator iter = in.begin();
		iter != in.end();
		iter++)
	{
		if ((*iter) == L' ' && !escaped)
		{
			out.push_back(currentToken);
			currentToken.clear();
		}
		else if ((*iter) == L'"')
		{
			escaped = !escaped;
		}
		else
		{
			currentToken.push_back((*iter));
		}
	}
	if (!currentToken.empty())
	{
		out.push_back(currentToken);
	}
	return out.size();
}

bool Utils::deleteOSfile(std::wstring filename)
{
	return 0 == DeleteFileW(filename.c_str());
}

std::wstring ConflictlessHashProvider::getHash(const std::wstring& seed)
{
	size_t hash = _generator(seed);

	while (_alreadyUsedHashes.find(hash) != _alreadyUsedHashes.end())
	{
		hash = _generator(seed + std::to_wstring(hash));
		_conflictTracker++;
	}

	_alreadyUsedHashes.insert(hash);
	std::wstringstream stream;
	stream << std::hex << hash;
	return stream.str();
}

std::wstring FsPath::getFullPath()
{
	std::wstring s;
	bool first = true;
	for (std::vector<CaseInsensitiveWstring>::iterator iter = _path.begin();
		iter != _path.end();
		iter++)
	{
		if (first)
		{
			first = false;
		}
		else
		{
			s += L"\\";
		}
		s += iter->getActualCase();
	}
	return s;
}

void ParalellReadUniqueWriteSync::getReadAccess()
{
	std::mutex mtx;
	std::unique_lock<std::mutex> lck(mtx);
	_statusMtx.lock();
	while (_writeAccess || _writeRequest)
	{
		_statusMtx.unlock();
		writeEndEvent.wait(lck);
		_statusMtx.lock();
	}
	_readAccesses++;
	_statusMtx.unlock();
}

void ParalellReadUniqueWriteSync::endReadAccess()
{
	_statusMtx.lock();
	_readAccesses--;
	_statusMtx.unlock();
	readEndEvent.notify_all();
}

void ParalellReadUniqueWriteSync::getWriteAccess()
{
	_writeMtx.lock();
	std::mutex mtx;
	std::unique_lock<std::mutex> lck(mtx);
	_statusMtx.lock();
	_writeRequest = true;
	while (_readAccesses > 0)
	{
		_statusMtx.unlock();
		readEndEvent.wait(lck);
		_statusMtx.lock();
	}
	_writeAccess = true;
	_writeRequest = false;
	_statusMtx.unlock();
}

void ParalellReadUniqueWriteSync::endWriteAccess()
{
	_statusMtx.lock();
	_writeAccess = false;
	_statusMtx.unlock();
	writeEndEvent.notify_all();
	_writeMtx.unlock();
}

bool operator<(const FsPath& rhs, const FsPath& lhs)
{
	if (rhs._path.size() == lhs._path.size())
	{
	for (size_t i = 0; i < rhs._path.size(); i++)
	{
		if (rhs._path[i] != lhs._path[i])
		{
			return rhs._path[i] < lhs._path[i];
		}
	}
	}
	return rhs._path.size() < lhs._path.size();
}


std::string Utils::getAvailableDrives()
{
	std::string retstr;
	DWORD driveVector = GetLogicalDrives();
	DWORD bitmask = 1;
	for (char letter = 'A'; letter < 'Z'; letter++)
	{
		if (!(driveVector & bitmask)) { retstr.append(1, letter); }
		bitmask = bitmask << 1;
	}
	return retstr;
}

CacheLocationProvider& CacheLocationProvider::getInstance()
{
	if (_instance == NULL)
	{
		_instance = new CacheLocationProvider();
	}
	return *_instance;
}

CacheLocationProvider::CacheLocationProvider()
{
	_cacheLocation = getRedirectedPath();
	if (_cacheLocation == L"")
	{
		_cacheLocation = getAppdataPath();
		if (_cacheLocation == L"")
		{
			throw std::exception("No cache directory");
		}
	}
}

std::wstring CacheLocationProvider::getAppdataPath()
{
	DWORD allocate = GetTempPathW(0, NULL);
	WCHAR* path = new WCHAR[allocate + 1];
	DWORD error = GetTempPathW(allocate, path);

	std::wstring pathWstr(path);
	delete path;
	if (error == 0) { return L""; }
	pathWstr.append(L"\\mtpmountcache");

	if (isMine(pathWstr)) { return pathWstr + L"\\";  }

	if (!Utils::checkDirectoryExistance(pathWstr))
	{
		if (!Utils::createOSDirectory(pathWstr)) { return L""; }
	}

	if (!Utils::isDirectoryEmpty(pathWstr))
	{
		return L"";
	}

	return makeItMine(pathWstr) ? pathWstr + L"\\" : L"";
}

std::wstring CacheLocationProvider::getRedirectedPath()
{
	std::wstring execPath = Utils::getExecutablePath();
	if (execPath.substr(execPath.size() - 4, std::string::npos) != L".exe")
	{
		return L"";
	}

	std::wstring configPath = execPath.substr(0, execPath.size() - 4) + L".cfg";
	std::wifstream configFile(configPath);
	if (!configFile.good())
	{
		return L"";
	}
	std::wstring redirectPath;
	std::getline(configFile, redirectPath);

	if (!Utils::checkDirectoryExistance(redirectPath))
	{
		if (!Utils::createOSDirectory(redirectPath)) { return L""; }
	}

	if (isMine(redirectPath)) { return redirectPath + L"\\"; }

	return makeItMine(redirectPath) ? redirectPath + L"\\" : L"";
}

bool CacheLocationProvider::makeItMine(std::wstring dir)
{
	std::ofstream idfileoff(dir + L"\\.mtpmountcache");
	std::string idfileoffcont("This file identifies the cache directory for mtpmount application.Do not remove it unless you are removing the directory it is in!");
	idfileoff.write(idfileoffcont.c_str(), idfileoffcont.size());
	idfileoff.close();

	std::ifstream check(dir + L"\\.mtpmountcache");
	if (!check.good()) { return false; }
	return true;
}

bool CacheLocationProvider::isMine(std::wstring dir)
{
	//check this directory for our identifier file
	std::ifstream idfile(dir + L"\\.mtpmountcache");
	if (idfile.good())
	{
		std::unordered_set<std::wstring> exceptions;
		exceptions.insert(L".mtpmountcache");
		Utils::deleteAllOSDirCont(dir, exceptions);
		return true;
	}
	return false;
}

std::wstring Utils::getExecutablePath()
{
	int bufsize = 256;
	WCHAR* buf = new WCHAR[bufsize];
	while (GetModuleFileNameW(NULL, buf, bufsize) == bufsize)
	{
		delete buf;
		bufsize *= 2;
		buf = new WCHAR[bufsize];
	}
	std::wstring execPath(buf);
	delete buf;
	return execPath;
}

CacheLocationProvider* CacheLocationProvider::_instance = NULL;