#pragma once

//The TestFileSystem is not an actual file system. It's just here for testing!

#include "AbstractFileSystem.h"
#include <Windows.h>

class TestFsNode : public AbstractFsNode
{
protected:
	TestFsNode(std::wstring friendlyname, AbstractFsNode* parent) : AbstractFsNode( friendlyname, parent) { }
public:
	virtual bool deleteContentByName(std::wstring name) { return false; } // directories only
	virtual bool deleteMe();
	virtual unsigned long long getUniqueId() { return 0; }
	virtual bool moveNode(AbstractFsNode* destDir);
	virtual bool rename(std::wstring newName);
};

class TestFsFile : public TestFsNode
{
private:
	static const int blocksize;
	typedef char byte;
	struct block
	{
		unsigned int size;
		byte* data;
	};
	std::vector<block> blocks;
public:
	TestFsFile(std::wstring& friendlyname, AbstractFsNode* parent) : TestFsNode(friendlyname, parent) { }
	virtual ~TestFsFile();
	virtual bool isDirectory() { return false; }
	virtual bool contentToStream(std::ostream& stream);
	virtual bool DEPRECATED StreamToContent(std::istream& stream) { return false; }
	bool fillWithData(std::istream& stream, unsigned long long size);
	virtual unsigned long long getPayloadSize()
	{
		unsigned long long size = 0;
		for (std::vector<block>::iterator iter = blocks.begin(); iter != blocks.end(); iter++)
		{
			size += iter->size;
		}
		return size;
	}
};

class TestFsDirectory : public TestFsNode
{
private:
	std::vector<AbstractFsNode*> _children;
public:
	TestFsDirectory(std::wstring& friendlyname, AbstractFsNode* parent) : TestFsNode(friendlyname, parent) { }
	virtual ~TestFsDirectory();
	virtual bool isDirectory() { return true; }
	virtual void deleteSubCache() { } //This file system does not cache anything
	virtual AbstractFsNode* getContentByName(CaseInsensitiveWstring subdirName);
	virtual bool DEPRECATED getDirContent(std::vector<AbstractFsNode*>& dirContent, unsigned int from = 0, unsigned int until = UINT_MAX) { return false; } //deprecated, so it doesn't work
	virtual bool doForAllContent(DoForAllDirContentFuctionObj& doWhat);
	virtual bool createNewFolder(CaseInsensitiveWstring name);
	virtual bool createNewFileWithStream(CaseInsensitiveWstring name, std::istream& stream, unsigned long long size, FILETIME modified, FILETIME created, FILETIME authored);
	virtual bool deleteContentByName(std::wstring name);
	void addChild(AbstractFsNode* newchild) { _children.push_back(newchild); }
	bool removeChild(AbstractFsNode* child);
};