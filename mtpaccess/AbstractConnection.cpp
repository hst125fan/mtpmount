#include "../dokanconnect/MemLeakDetection.h"
#include "AbstractConnection.h"

AbstractFsNode* AbstractMappableDrive::_filePathToNode(FsPath& path)
{
	AbstractFsNode* currentNode = driveRoot;
	for (int i = 0; i < path.getPathLength(); i++)
	{
		currentNode = currentNode->getContentByName(path.getPathOnLayer(i));
		if (currentNode == NULL) { return NULL; }
	}
	return currentNode;
}

AbstractMappableDrive::~AbstractMappableDrive()
{
	delete driveRoot;
}

AbstractMappableDrive::checkIfExistsReturn AbstractMappableDrive::checkIfExists(FsPath dirOrFilePath)
{
	AbstractFsNode* fsnode = _filePathToNode(dirOrFilePath);
	if (fsnode == NULL) { return DOES_NOT_EXIST; }
	return (fsnode->isDirectory()) ? IS_DIRECTORY : IS_FILE;
}

bool AbstractMappableDrive::readFile(FsPath filePath, std::ostream& destination)
{
	AbstractFsNode* fsnode = _filePathToNode(filePath);
	if (fsnode == NULL || fsnode->isDirectory())
	{
		return false;
	}
	return fsnode->contentToStream(destination);
}

bool AbstractMappableDrive::writeFile(FsPath filePath, std::istream& source, unsigned long long size)
{
	AbstractFsNode* fsnode = _filePathToNode(filePath);
	if (fsnode == NULL || fsnode->isDirectory())
	{
		return false;
	}

	AbstractFsNode* parent = fsnode->getParent();
	if (parent == NULL)
	{
		return false;
	}
	FILETIME authored = fsnode->getCreationTime();
	FILETIME created = fsnode->getCreationTime();
	FILETIME modified = fsnode->getModificationTime();

	std::wstring name = fsnode->getNodeName();
	if (!fsnode->deleteMe())
	{
		return false;
	}

	parent->deleteSubCache(); //from now on, fsnode pointer is invalid!
	bool fileCreation = parent->createNewFileWithStream(name, source, size, modified, created, authored);
	parent->deleteSubCache();

	AbstractFsNode* fsnode2 = _filePathToNode(filePath);
	if (fsnode2 != NULL)
	{
		fsnode2->syncFileTime(modified, authored, created);
	}

	return fileCreation;
}


bool AbstractMappableDrive::createFileWithStream(FsPath baseDirectory, std::wstring filename, std::istream& source, unsigned long long size)
{
	AbstractFsNode* fsnode = _filePathToNode(baseDirectory);
	if (fsnode == NULL || !fsnode->isDirectory())
	{
		return false;
	}
	FILETIME filetime = { 0,0 };
	SYSTEMTIME systime = { 0 };
	GetSystemTime(&systime);
	SystemTimeToFileTime(&systime, &filetime);
	bool ret = fsnode->createNewFileWithStream(filename, source, size, filetime, filetime, filetime);
	if (ret)
	{
		fsnode->deleteSubCache();
	}
	return ret;
}

unsigned long long AbstractMappableDrive::getFileSize(FsPath filePath)
{
	AbstractFsNode* fsnode = _filePathToNode(filePath);
	if (fsnode == NULL || !fsnode->isDirectory())
	{
		return 0;
	}
	return fsnode->getPayloadSize();
}


bool AbstractMappableDrive::deleteFileOrDirectory(FsPath dirOrFilePath, bool expectFile)
{
	AbstractFsNode* fsnode = _filePathToNode(dirOrFilePath);
	if (fsnode == NULL || (fsnode->isDirectory() && expectFile))
	{
		return false;
	}

	AbstractFsNode* parent = fsnode->getParent();
	if (parent == NULL)
	{
		return false;
	}

	bool deletion = fsnode->deleteMe();
	parent->deleteSubCache();
	return deletion;
}


bool AbstractMappableDrive::createFolder(FsPath baseDirectory, std::wstring name)
{
	AbstractFsNode* fsnode = _filePathToNode(baseDirectory);
	if (fsnode == NULL)
	{
		return false;
	}
	if (fsnode->createNewFolder(name))
	{
		fsnode->deleteSubCache();
		return true;
	}
	return false;
}

bool DEPRECATED AbstractMappableDrive::discoverDirectory(FsPath dirPath, std::vector<AbstractFsReadOnlyNode*>& content, unsigned int from, unsigned int until)
{
	content.clear();
	AbstractFsNode* dir = _filePathToNode(dirPath);
	if (dir == NULL || !dir->isDirectory())
	{
		return false;
	}

	std::vector<AbstractFsNode*> fsnodes;
	bool success = dir->getDirContent(fsnodes, from, until); // this method is deprecated
	for (std::vector<AbstractFsNode*>::iterator iter = fsnodes.begin();
		iter != fsnodes.end();
		iter++)
	{
		content.push_back(*iter);
	}
	return success;
}

bool AbstractMappableDrive::doForAllDirContent(FsPath dirPath, DoForAllDirContentFuctionObj& dirContFunc)
{
	AbstractFsNode* dir = _filePathToNode(dirPath);
	if (dir == NULL || !dir->isDirectory())
	{
		return false;
	}
	return dir->doForAllContent(dirContFunc);
}

bool AbstractMappableDrive::doForSingleObject(FsPath objPath, DoForAllDirContentFuctionObj& singleObjFunc)
{
	AbstractFsNode* node = _filePathToNode(objPath);
	if (node == NULL)
	{
		return false;
	}
	singleObjFunc.doForAllDirContentMethod(node);
	return true;
}

bool AbstractMappableDrive::renameDirectory(FsPath oldpath, FsPath newpath)
{
	return false;
}

bool AbstractMappableDrive::renameObject(FsPath object, CaseInsensitiveWstring newName)
{
	AbstractFsNode* node = _filePathToNode(object);
	FsPath newPath( object.getLeftSubPath(object.getPathLength() - 2).getFullPath() + L"\\" + newName.getActualCase());
	AbstractFsNode* mustBeNull = _filePathToNode(newPath);
	if (node == NULL || mustBeNull != NULL)
	{
		return false;
	}

	return node->rename(newName.getActualCase());
}

bool AbstractMappableDrive::moveNode(FsPath nodeOldPath, FsPath nodeNewPath)
{
	AbstractFsNode* node = _filePathToNode(nodeOldPath);
	AbstractFsNode* thisNodeMustBeFree = _filePathToNode(nodeNewPath);
	if (node == NULL || thisNodeMustBeFree != NULL)
	{
		return false;
	}

	bool mustMove = false;
	if (nodeOldPath.getPathLength() == nodeNewPath.getPathLength())
	{
		for (int i = 0; i < nodeOldPath.getPathLength() - 1; i++)
		{
			if (nodeOldPath.getPathOnLayer(i) != nodeNewPath.getPathOnLayer(i))
			{
				mustMove = true;
				break;
			}
		}
	}
	else
	{
		mustMove = true;
	}

	if (mustMove)
	{
		//FsPath destDirPath = nodeNewPath.getLeftSubPath(nodeNewPath.getPathLength() - 2);
		return internalMoveNode(nodeOldPath, nodeNewPath, CacheLocationProvider::getInstance().getIntermediateStore());
	}

	FsPath pathAfterInitialMove(nodeNewPath.getLeftSubPath(nodeNewPath.getPathLength() - 2).getFullPath() + L"\\" + nodeOldPath.getPathOnLayer(nodeOldPath.getPathLength() - 1).getActualCase());
	AbstractFsNode* nodeAfterMove = _filePathToNode(pathAfterInitialMove);
	if (nodeAfterMove == NULL)
	{
		return false;
	}

	if (pathAfterInitialMove.getPathOnLayer(pathAfterInitialMove.getPathLength() - 1) != nodeNewPath.getPathOnLayer(nodeNewPath.getPathLength() - 1))
	{
		return nodeAfterMove->rename(nodeNewPath.getPathOnLayer(nodeNewPath.getPathLength() - 1).getActualCase());
	}
	else
	{
		return true;
	}
}

bool AbstractMappableDrive::internalMoveNode(FsPath nodeOldPath, FsPath nodeNewPath, std::wstring intermediateStoreFile)
{
	AbstractFsNode* oldNode = _filePathToNode(nodeOldPath);
	AbstractFsNode* newNode = _filePathToNode(nodeNewPath);
	FsPath newBasePath(nodeNewPath.getLeftSubPath(nodeNewPath.getPathLength() - 2));
	AbstractFsNode* newBaseNode = _filePathToNode(newBasePath);
	if (newNode != NULL || oldNode == NULL || newBaseNode == NULL)
	{
		return false;
	}

	if (!oldNode->isDirectory())
	{
		//Files
		std::ofstream tempCopy(intermediateStoreFile, std::ios::binary);
		bool gotFile = oldNode->contentToStream(tempCopy);
		tempCopy.close();
		std::ifstream tempCopyBack(intermediateStoreFile, std::ios::binary);
		bool pushedFile = newBaseNode->createNewFileWithStream(CaseInsensitiveWstring(nodeNewPath.getPathOnLayer(nodeNewPath.getPathLength() - 1)), tempCopyBack, Utils::getOSFileSize(intermediateStoreFile));
		tempCopyBack.close();
		Utils::deleteOSfile(intermediateStoreFile);

		if (gotFile && pushedFile)
		{
			AbstractFsNode* parent = oldNode->getParent();
			oldNode->deleteMe();
			newBaseNode->deleteSubCache();
			parent->deleteSubCache();
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		//Directories
		if (!newBaseNode->createNewFolder(CaseInsensitiveWstring(nodeNewPath.getPathOnLayer(nodeNewPath.getPathLength() - 1)))) { return false; }
		AbstractFsNode* newFolder = _filePathToNode(nodeNewPath);
		if (newFolder == NULL) { return false; }

		CopyAllStuffThatIsInsideADirectory copycat(newFolder, intermediateStoreFile, this);
		oldNode->doForAllContent(copycat);
		copycat.tidyUp();

		if (copycat.isFailed())
		{
			oldNode->deleteSubCache();
			newBaseNode->deleteSubCache();
			return false;
		}
		else
		{
			AbstractFsNode* parent = oldNode->getParent();
			oldNode->deleteMe();
			parent->deleteSubCache();
			newBaseNode->deleteSubCache();
			return true;
		}
	}
}

bool CopyAllStuffThatIsInsideADirectory::doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
{
	if (currentNode->isDirectory())
	{
		//Directories: recursive call
		_destDir->createNewFolder(CaseInsensitiveWstring(currentNode->getNodeName()));
		FsPath newFolderPath(_destDir->getNodePath() + L"\\" + currentNode->getNodeName());
		AbstractFsNode* newlyCreatedFolder = _caller->_filePathToNode(newFolderPath);
		if (newlyCreatedFolder != NULL)
		{
			CopyAllStuffThatIsInsideADirectory copycat(newlyCreatedFolder, _tempfile, _caller);
			currentNode->doForAllContent(copycat);
			copycat.tidyUp();

			if (copycat.isFailed())
			{
				_error = true;
			}
			else
			{
				AbstractFsNode* hackElevateMyAccessRights = dynamic_cast<AbstractFsNode*>(currentNode);
				_deleteThem.push_back(hackElevateMyAccessRights);
			}
		}
		else
		{
			_error = true;
		}
		return true;
	}
	else
	{
		//Files: copy them!
		AbstractFsNode* hackElevateMyAccessRights = dynamic_cast<AbstractFsNode*>(currentNode);
		if (hackElevateMyAccessRights != NULL)
		{
			std::ofstream tempCopy(_tempfile, std::ios::binary);
			bool gotFile = hackElevateMyAccessRights->contentToStream(tempCopy);
			tempCopy.close();
			std::ifstream tempCopyBack(_tempfile, std::ios::binary);
			bool pushedFile = _destDir->createNewFileWithStream(CaseInsensitiveWstring(hackElevateMyAccessRights->getNodeName()), tempCopyBack, Utils::getOSFileSize(_tempfile));
			tempCopyBack.close();
			Utils::deleteOSfile(_tempfile);
			if (gotFile && pushedFile)
			{
				_deleteThem.push_back(hackElevateMyAccessRights);
			}
			else
			{
				_error = true;
			}
		}
		return true;
	}
}

void CopyAllStuffThatIsInsideADirectory::tidyUp()
{
	for (std::vector<AbstractFsNode*>::iterator iter = _deleteThem.begin();
		iter != _deleteThem.end();
		iter++)
	{
		(*iter)->deleteMe();
	}
}

