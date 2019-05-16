#include "../dokanconnect/MemLeakDetection.h"
#include "TestFileSystem.h"

TestFsFile::~TestFsFile()
{
	for (std::vector<block>::iterator iter = blocks.begin();
		iter != blocks.end();
		iter++)
	{
		delete iter->data;
		iter->data = NULL;
	}
}

bool TestFsFile::contentToStream(std::ostream& stream)
{
	for (std::vector<block>::iterator iter = blocks.begin();
		iter != blocks.end();
		iter++)
	{
		for (unsigned int i = 0; i < iter->size; i++)
		{
			stream.put(*(iter->data + i));
		}
	}
	return true;
}

bool TestFsFile::fillWithData(std::istream& stream, unsigned long long size)
{
	unsigned long long fullBlockCnt = size / blocksize;
	int lastBlockSize = size % blocksize;
	for (int i = 0; i < fullBlockCnt; i++)
	{
		block newblock;
		newblock.size = blocksize;
		newblock.data = new byte[blocksize];
		for (int j = 0; j < blocksize; j++)
		{
			newblock.data[j] = stream.get();
		}
		blocks.push_back(newblock);
	}
	
	block lastblock;
	lastblock.size = lastBlockSize;
	lastblock.data = new byte[lastBlockSize];
	for (int i = 0; i < lastBlockSize; i++)
	{
		lastblock.data[i] = stream.get();
	}
	blocks.push_back(lastblock);

	return true;
}

bool TestFsNode::deleteMe()
{
	std::wstring myName = _friendlyname;
	if (_parent == NULL)
	{
		return false;
	}
	TestFsNode* moreConc = dynamic_cast<TestFsNode*>(_parent);
	if (moreConc == NULL)
	{
		return false;
	}
	return moreConc->deleteContentByName(myName);
}

const int TestFsFile::blocksize = 512;


TestFsDirectory::~TestFsDirectory()
{
	for (std::vector<AbstractFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		delete (*iter);
	}
}

AbstractFsNode* TestFsDirectory::getContentByName(CaseInsensitiveWstring subdirName)
{
	for (std::vector<AbstractFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if (CaseInsensitiveWstring((*iter)->getNodeName()) == subdirName)
		{
			return (*iter);
		}
	}
	return NULL;
}

bool TestFsDirectory::doForAllContent(DoForAllDirContentFuctionObj& doWhat)
{
	for (std::vector<AbstractFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if (!doWhat.doForAllDirContentMethod(*iter))
		{
			return true;
		}
	}
	return true;
}

bool TestFsDirectory::createNewFolder(CaseInsensitiveWstring name)
{
	if (getContentByName(name) != NULL)
	{
		return false;
	}
	else
	{
		std::wstring namestring = name.getActualCase();
		TestFsDirectory* newdir = new TestFsDirectory(namestring, this);
		_children.push_back(newdir);
		return true;
	}
}

bool TestFsDirectory::createNewFileWithStream(CaseInsensitiveWstring name, std::istream& stream, unsigned long long size, FILETIME modified, FILETIME created, FILETIME authored)
{
	if (getContentByName(name) != NULL)
	{
		return false;
	}
	else
	{
		std::wstring namestring = name.getActualCase();
		TestFsFile* newfile = new TestFsFile(namestring, this);
		newfile->setAuthorizationTime(authored);
		newfile->setCreationTime(created);
		newfile->setModificationTime(modified);
		if (newfile->fillWithData(stream, size))
		{
			_children.push_back(newfile);
			return true;
		}
		else
		{
			delete newfile;
			return false;
		}
	}
}

bool TestFsDirectory::deleteContentByName(std::wstring name)
{
	for (std::vector<AbstractFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if ((*iter)->getNodeName() == name)
		{
			delete (*iter);
			_children.erase(iter);
			return true;
		}
	}
	return false;
}

bool TestFsDirectory::removeChild(AbstractFsNode* child)
{
	for (std::vector<AbstractFsNode*>::iterator iter = _children.begin();
		iter != _children.end();
		iter++)
	{
		if ((*iter) == child)
		{
			_children.erase(iter);
			return true;
		}
	}
	return false;
}

bool TestFsNode::moveNode(AbstractFsNode* destDir)
{
	TestFsDirectory* dest = dynamic_cast<TestFsDirectory*>(destDir);
	if (dest == NULL) { return false; }

	TestFsDirectory* parent = dynamic_cast<TestFsDirectory*>(this->_parent);
	if (parent == NULL) { return false; }
	if (!parent->removeChild(this)) { return false; }
	dest->addChild(this);
	_parent = dest;
	return true;
}

bool TestFsNode::rename(std::wstring newName)
{
	_friendlyname = newName;
	return true;
}