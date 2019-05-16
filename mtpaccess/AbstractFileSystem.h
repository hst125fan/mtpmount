#pragma once
#include <string>
#include <Windows.h>
#include "Utils.h"

class DoForAllDirContentFuctionObj;

class AbstractFsReadOnlyNode
{
public:
	virtual bool isDirectory() = 0;
	virtual std::wstring getNodeName() = 0;
	virtual bool mayDelete() = 0;
	virtual unsigned long long getPayloadSize() = 0;
	virtual unsigned long long getUniqueId() = 0;
	virtual std::wstring getNodePath() = 0;
	virtual bool doForAllContent(DoForAllDirContentFuctionObj& doWhat) = 0;
	virtual FILETIME getCreationTime() = 0;
	virtual FILETIME getModificationTime() = 0;
	virtual FILETIME getAuthorizationTime() = 0;
};

//Function object for AbstractFsNode::doForAllContent method
class DoForAllDirContentFuctionObj
{
public:
	//This method will be called with every object (node) inside the discovered directory
	//return true if you want to continue, false if you want to terminate the calling process
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode) = 0;
};

class AbstractFsNode : public AbstractFsReadOnlyNode
{
protected:
	std::wstring _friendlyname;
	AbstractFsNode* _parent;
	bool _allowedToDelete;
	FILETIME _timeCreated;
	FILETIME _timeModified;
	FILETIME _timeAuthored;
	AbstractFsNode(std::wstring& friendlyname, AbstractFsNode* parent) : _friendlyname(friendlyname), _parent(parent), _allowedToDelete(true), _timeCreated({ 0,0 }), _timeModified({ 0,0 }), _timeAuthored({ 0,0 }) { }
public:
	virtual void deleteSubCache() { } //implement for directories only
	virtual bool deleteMe() = 0;
	std::wstring getNodeName() { return _friendlyname; }
	virtual AbstractFsNode* getContentByName(CaseInsensitiveWstring subdirName) { return NULL; } //implement for directories only
	std::wstring getNodePath() { if (_parent == NULL) { return this->getNodeName(); }
								 else { return _parent->getNodePath() + L"\\" + this->getNodeName(); } }
	virtual bool DEPRECATED getDirContent(std::vector<AbstractFsNode*>& dirContent, unsigned int from = 0, unsigned int until = UINT_MAX) { return false; } //implement for directories only
	virtual bool doForAllContent(DoForAllDirContentFuctionObj& doWhat) { return false; } //implement for directories only
	virtual bool contentToStream(std::ostream& stream) { return false; } //implement for files only
	virtual bool DEPRECATED StreamToContent(std::istream& stream) { return false; } //implement for files only
	virtual bool createNewFileWithStream(CaseInsensitiveWstring name, std::istream& stream, unsigned long long size, FILETIME modified = { 0,0 }, FILETIME created = { 0,0 }, FILETIME authored = { 0,0 }) { return false; } //implement for directories only
	virtual bool createNewFolder(CaseInsensitiveWstring name) { return false; } //implement for directories only
	AbstractFsNode* getParent() { return _parent; }
	virtual ~AbstractFsNode() { }
	void disallowDeletion() { _allowedToDelete = false; }
	virtual bool mayDelete() { return _allowedToDelete; }
	virtual unsigned long long getPayloadSize() { return 0; } //implement for files only
	//virtual bool rename(FsPath newpath) { return false; } //not implemented yet
	virtual bool moveNode(AbstractFsNode* destDir) = 0;
	virtual bool rename(std::wstring newName) = 0;
	virtual FILETIME getCreationTime() { return _timeCreated; }
	virtual FILETIME getModificationTime() { return _timeModified; }
	virtual FILETIME getAuthorizationTime() { return _timeAuthored; }
	void setCreationTime(FILETIME time) { _timeCreated = time; }
	void setModificationTime(FILETIME time) { _timeModified = time; }
	void setAuthorizationTime(FILETIME time) { _timeAuthored = time; }
	virtual void syncFileTime(FILETIME modified, FILETIME created, FILETIME authored) { }
};