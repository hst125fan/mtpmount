#pragma once
#include "AbstractFileSystem.h"

class AbstractMappableDrive;

class AbstractConnection
{
private:
	std::wstring _deletekey;
	int _refcount;
protected:
	AbstractConnection(std::wstring& deletekey) : _deletekey(deletekey) { _refcount = 1; }
public:
	std::wstring getDeleteKey() { return _deletekey; }
	int getRefcount() { return _refcount; }
	void refAdd() { _refcount++; }
	void refDel() { _refcount--; }
	virtual ~AbstractConnection() { }

	//Obtain a list of all names of storage media found in the device
	//Parameters:
	//(1) OUT	std::vector<std::wstring>& putNamesHere		all the names
	//(2) IN	bool forceRefresh (OPTIONAL)				do a refresh on the connection to recognize changes
	//Returns: true on success, false on failure
	virtual bool getMappableDriveNames(std::vector<std::wstring>& putNamesHere, bool forceRefresh = false) = 0;

	//Obtain the count of storage media found in the device
	//Parameters:
	//(1) OUT	int& putCountHere				the count you asked for
	//(2) IN	bool forceRefresh (OPTIONAL)	do a refresh on the connection to recognize changes
	//Returns: true on success, false on failure
	virtual bool getMappableDriveCnt(int& putCountHere, bool forceRefresh = false) = 0;

	//search for storage media given its friendly name
	//Parameters:
	//(1) IN	std::wstring friendlyname	the name we are searching for
	//Returns: the mappable drive object with matching name on success, NULL on failure.
	virtual AbstractMappableDrive* getMDriveByFName(std::wstring friendlyname) = 0;
};

//This class/interface will later be used to access the "mapped drive", which refers to the storage media on a MTP-connected device.
//The actual file system it uses is undefined here ("AbstractFsNode* driveRoot), therefore it cannot be instanced.
class AbstractMappableDrive
{
private:
	AbstractMappableDrive(AbstractMappableDrive const& p) = delete;
	AbstractMappableDrive& operator=(AbstractMappableDrive const& p) = delete;
	friend class CopyAllStuffThatIsInsideADirectory;
protected:
	AbstractFsNode* driveRoot;
	std::wstring _friendlyname;
	AbstractFsNode* _filePathToNode(FsPath& path);

	AbstractMappableDrive(std::wstring friendlyname) : _friendlyname(friendlyname) { }
public:
	enum checkIfExistsReturn
	{
		DOES_NOT_EXIST = 0,
		IS_DIRECTORY = 1,
		IS_FILE = 2
	};
	~AbstractMappableDrive();

	//get the friendly name of the file systems ROOT (=name of the storage media)
	//Returns: what you think it does
	std::wstring getFriendlyName() { return _friendlyname; }

	//gain access to the MtpFs (File System) of the storage media. This might be dangerous. Don't do it unless you know what you are doing!
	//Returns: storage media file system root
	AbstractFsNode* DEPRECATED getRawFs() { return driveRoot; }

	//check existance of a directory or a file matching the given file path
	//Parameters:
	//(1) IN	FsPath dirOrFilePath	path (from root) to the requested object. Can easily be created from a widestring.
	//Returns: Enum for file existance (MtpMappableDrive::checkIfExistsReturn): 0 on nonexistance, 1 if it's a folder, 2 if it's a file 
	checkIfExistsReturn checkIfExists(FsPath dirOrFilePath);

	//read content of a file on the device/mappable drive given its path
	//Parameters:
	//(1) IN	FsPath filePath				path (from root) to the requested file. Can easily be created from a widestring.
	//(2) IN	std::ostream& destination	output stream where the file content is written to. Binary output mode must be used (Filestream: create with flag std::ios::binary)
	//Returns: true on success, false on failure
	bool readFile(FsPath filePath, std::ostream& destination);

	//write content from an input stream to a file on the device given its path
	//Parameters:
	//(1) IN	FsPath filePath			path (from root) to the requested file. This file must already exist.
	//(2) IN	std::istream& source	input stream containing the file content.
	//(3) IN	unsigned long long size	byte count to be passed to the input stream
	//Returns: true on success, false on failure.
	bool writeFile(FsPath filePath, std::istream& source, unsigned long long size);

	//create a new file within an existing directory and write content directly to it. This directory must not contain a file with identical name.
	//Parameters:
	//(1) IN	FsPath baseDirectory	path to the directory the file should be placed in. This needs to be a vaild directory path. Do not give the desired path of the file here!
	//(2) IN	std::wstring filename	the name of the new-to-create file.
	//(3) IN	std::istream& source	input stream containing the file content.
	//(4) IN	unsigned long long size	amount of bytes to pass using the input stream (3).
	//Returns: true on success, false on failure.
	bool createFileWithStream(FsPath baseDirectory, std::wstring filename, std::istream& source, unsigned long long size);

	//delete a file or a directory on the device. On directory deletion, all files and sub-directories contained will also be deleted.
	//Parameters:
	//(1) IN	FsPath fileOrDirPath	path to the file or directory which should be deleted
	//(2) IN	bool expectFile			when set to true, the object is only deleted when it's a file
	//Returns: true on success, false on failure
	bool deleteFileOrDirectory(FsPath fileOrDirPath, bool expectFile = false);

	//create a new folder inside an existing directory
	//Parameters:
	//(1) IN	FsPath baseDirectory	path of the directory the new folder should be created in. This needs to be a valid directory path. Do not give the desired path of the new folder here!
	//(2) IN	std::wstring name		name of the new-to-create folder. Cannot be created on duplicate names.
	//Returns: true on success, false on failure
	bool createFolder(FsPath baseDirectory, std::wstring name);

	//obtain the content of a directory -- DEPRECATED
	//Parameters:
	//(1) IN	FsPath dirPath								path of the directory to discover
	//(2) OUT	std::vector<MtpFsReadOnlyNode*>& content	the content of the directory
	//(3) IN	int from									first index to evaluate (when discovering partly). Optional.
	//(4) IN	int until									last index to evaluate (when discovering partly). Optional.
	//Returns: true on success, false on failure
	bool DEPRECATED discoverDirectory(FsPath dirPath, std::vector<AbstractFsReadOnlyNode*>& content, unsigned int from = 0, unsigned int until = UINT_MAX);

	//do something for every object inside a given directory
	//Parameters:
	//(1) IN	FsPath dirPath								path of the directory
	//(2) INOUT	DoForAllDirContentFunctionObj dirContFunc	object containing the stuff to do as a method
	bool doForAllDirContent(FsPath dirPath, DoForAllDirContentFuctionObj& dirContFunc);

	//experimental: same as doForAllDirContent, but only on a single inode (file/dir)
	//Parameters:
	//(1) IN	FsPath dirPath								path of the file or directory
	//(2) INOUT	DoForAllDirContentFunctionObj dirContFunc	object containing the stuff to do as a method
	bool doForSingleObject(FsPath objPath, DoForAllDirContentFuctionObj& singleObjFunc);

	//DEPRECATED DO NOT USE
	bool DEPRECATED renameDirectory(FsPath oldName, FsPath newName);

	//Node move not supported by some devices. Therefore, don't do it!
	bool moveNode(FsPath nodeOldPath, FsPath nodeNewPath);

	bool renameObject(FsPath object, CaseInsensitiveWstring newName);

	bool internalMoveNode(FsPath nodeOldPath, FsPath nodeNewPath, std::wstring intermediateStoreFile);

	unsigned long long getFileSize(FsPath filePath);
};

class CopyAllStuffThatIsInsideADirectory : public DoForAllDirContentFuctionObj
{
private:
	AbstractFsNode* _destDir;
	std::wstring _tempfile;
	AbstractMappableDrive* _caller;
	bool _error;
	std::vector<AbstractFsNode*> _deleteThem;
public:
	CopyAllStuffThatIsInsideADirectory(AbstractFsNode* destDir, std::wstring tempfile, AbstractMappableDrive* caller) : _destDir(destDir), _tempfile(tempfile), _caller(caller), _error(false) { }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode);
	bool isFailed() { return _error; }
	void tidyUp();
};