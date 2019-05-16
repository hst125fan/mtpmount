#include "../dokanconnect/MemLeakDetection.h"
#include "ConnectionProvider.h"
#include "Utils.h"

#include <iostream>
#include <map>
#include <string>


typedef std::map<std::wstring, AbstractMappableDrive*> devDrvMap;
typedef std::map<std::wstring, devDrvMap*> devMap;

devMap allDrives;

class WriteAllToStdOut : public DoForAllDirContentFuctionObj
{
private:
	bool firstCall;
public:
	WriteAllToStdOut() { firstCall = true; }
	virtual bool doForAllDirContentMethod(AbstractFsReadOnlyNode* currentNode)
	{
		if (firstCall) { wprintf(L"Directory content:\n"); }
		firstCall = false;
		wprintf(L"%s %s (%lld Bytes)\n", (currentNode->isDirectory() ? L"D" : L"F"), currentNode->getNodeName().c_str(), currentNode->getPayloadSize());
		return true;
	}
};


void storageMain(AbstractMappableDrive* drv, std::wstring name)
{
	bool didIDoAnything = true;

	while (1)
	{
		std::wstring input;
		std::vector<std::wstring> cmd;
		if (didIDoAnything) { std::wcout << "MtpAccess[STORAGE:" << name << "]>"; }
		std::getline(std::wcin, input);
		Utils::tokenize(input, cmd);

		if (cmd.size() == 0)
		{
			didIDoAnything = false;
			continue;
		}
		didIDoAnything = true;
		if (cmd[0] == L"exit")
		{
			std::cout << "Storage mode exited by user" << std::endl;
			return;
		}
		else if (cmd[0] == L"do")
		{
			std::string execCmd = Utils::wstringToString(input).substr( 3 );
			system(execCmd.c_str());
			continue;
		}
		else if (cmd[0] == L"push" && cmd.size() == 3)
		{
			std::ifstream pcfile(cmd[2], std::ios::binary);
			if (!pcfile.good())
			{
				std::cout << "source file on PC could not be found" << std::endl;
				continue;
			}

			if (AbstractMappableDrive::IS_FILE != drv->checkIfExists(FsPath(cmd[1])))
			{
				std::cout << "File on device not found" << std::endl; 
				continue;
			}

			unsigned long long filesize = Utils::getOSFileSize(cmd[2]);
			bool success = drv->writeFile(FsPath(cmd[1]), pcfile, filesize);

			if (success)
			{
				std::cout << "File write successful" << std::endl;
			}
			else
			{
				std::cout << "File write failed" << std::endl;
			}
			continue;
		}

		else if (cmd[0] == L"pull" && cmd.size() == 3)
		{
			if (AbstractMappableDrive::IS_FILE != drv->checkIfExists(FsPath(cmd[1])))
			{
				std::cout << "No valid file path for device (source)" << std::endl;
				continue;
			}

			std::ofstream pcfile(cmd[2], std::ios::binary);
			if (!pcfile.good())
			{
				std::cout << "Output file could not be created" << std::endl;
				continue;
			}

			bool success = drv->readFile(FsPath(cmd[1]), pcfile);
			if (success)
			{
				std::cout << "File successfully transferred" << std::endl;
			}
			else
			{
				std::cout << "Error while transfering file" << std::endl;
			}
			continue;
		}
		else if (cmd[0] == L"newdir" && cmd.size() == 2)
		{
			FsPath targetPath(cmd[1]);
			bool error = false;
			for (int i = 0; i < targetPath.getPathLength(); i++)
			{
				if (!error)
				{
					FsPath pathToThisLayer = targetPath.getLeftSubPath(i);
					switch (drv->checkIfExists(pathToThisLayer))
					{
						case AbstractMappableDrive::IS_DIRECTORY: break;
						case AbstractMappableDrive::IS_FILE: error =  true; break;
						case AbstractMappableDrive::DOES_NOT_EXIST: 
						{
							FsPath newbase = targetPath.getLeftSubPath(i - 1);
							std::wstring newname = targetPath.getPathOnLayer(i).getActualCase();
							if (!drv->createFolder(newbase, newname)) { error = true; }
							break;
						}
					}
				}
			}
			if (error)
			{
				std::cout << "Directory could not be created!" << std::endl;
			}
			else
			{
				std::cout << "Directory successfully created" << std::endl;
			}
			continue;
		}
		else if (cmd[0] == L"newfile" && cmd.size() == 3)
		{
			FsPath target(cmd[1]);
			if (target.getPathLength() < 1)
			{
				std::cout << "This is not a valid path" << std::endl; 
				continue;
			}
			if (AbstractMappableDrive::DOES_NOT_EXIST != drv->checkIfExists(target))
			{
				std::cout << "File already exists" << std::endl;
				continue;
			}
			std::ifstream input(cmd[2], std::ios::binary);
			if (!input.good())
			{
				std::cout << "Input (PC File) not found" << std::endl;
				continue;
			}
			FsPath base = target.getLeftSubPath(target.getPathLength() - 2);
			std::wstring name = target.getPathOnLayer(target.getPathLength() - 1).getActualCase();
			unsigned long long size = Utils::getOSFileSize(cmd[2]);
			bool success = drv->createFileWithStream(base, name, input, size);
			if (success)
			{
				std::cout << "File created successfully" << std::endl;
			}
			else
			{
				std::cout << "File creation on device failed" << std::endl;
			}
		}
		else if (cmd[0] == L"dir" && cmd.size() == 2)
		{
			if (AbstractMappableDrive::IS_DIRECTORY != drv->checkIfExists(FsPath(cmd[1])))
			{
				std::cout << "This path is not a valid directory" << std::endl;
				continue;
			}

			WriteAllToStdOut s;
			if (!drv->doForAllDirContent(FsPath(cmd[1]), s))
			{
				std::cout << "Directory discovery failed!" << std::endl;
			}
			continue;
		}
		else if (cmd[0] == L"delete" && cmd.size() == 2)
		{
			FsPath target(cmd[1]);
			if (AbstractMappableDrive::DOES_NOT_EXIST == drv->checkIfExists(target))
			{
				std::cout << "This file or directory does not exist" << std::endl;
				continue;
			}
			bool success = drv->deleteFileOrDirectory(target, false);
			if (success)
			{
				std::cout << "Deletion successful" << std::endl;
			}
			else
			{
				std::cout << "Deleting failed" << std::endl;
			}
			continue;
		}
		else if (cmd[0] == L"internalmove" && cmd.size() == 3)
		{
			FsPath origin(cmd[1]);
			FsPath target(cmd[2]);
			if (AbstractMappableDrive::DOES_NOT_EXIST == drv->checkIfExists(origin))
			{
				std::cout << "This file or directory does not exist" << std::endl;
				continue;
			}
			bool success = drv->moveNode(origin, target);
			if (success)
			{
				std::cout << "Renaming/moving successful" << std::endl;
			}
			else
			{
				std::cout << "Renaming/moving failed" << std::endl;
			}
			continue;
		}
		else
		{
			std::cout << "Command not found, or wrong parameters" << std::endl << std::endl;

			std::cout << "Available commands (storage mode):" << std::endl;
			std::cout << "delete <device-path>                      delete a file or directory (recursive) on the device" << std::endl;
			std::cout << "dir <device-dir-path>                     show content of a directory on the device" << std::endl;
			std::cout << "do <command>                              execute an external program or command" << std::endl;
			std::cout << "newdir <device-path>					    create a new directory on the device" << std::endl;
			std::cout << "newfile <device-file-path> <pc-file-path> transfer a file from PC which does not yet exist on the device" << std::endl;
			std::cout << "pull <device-file-path> <pc-file-path>    transfer a file from device to PC" << std::endl;
			std::cout << "push <device-file-path> <pc-file-path>    transfer a file from PC to device" << std::endl << std::endl;
		}
	}
}

void deviceMain(AbstractConnection* conn, std::wstring name)
{
	devDrvMap* myDrives;
	try
	{
		myDrives = allDrives.at(name);
	}
	catch (std::out_of_range)
	{
		myDrives = new devDrvMap;
		allDrives.emplace(name, myDrives);
	}

	bool didIDoAnything = true;

	while (1)
	{
		std::wstring input;
		std::vector<std::wstring> cmd;
		if (didIDoAnything) { std::wcout << "MtpAccess[DEVICE:" << name << "]>" ; }
		std::getline(std::wcin, input);
		Utils::tokenize(input, cmd);

		if (cmd.size() == 0)
		{
			didIDoAnything = false;
			continue;
		}
		didIDoAnything = true;
		if (cmd[0] == L"exit")
		{
			std::cout << "Device mode exited by user" << std::endl;
			return;
		}
		else if (cmd[0] == L"do")
		{
			std::string execCmd = Utils::wstringToString(input).substr(3);
			system(execCmd.c_str());
			continue;
		}
		else if (cmd[0] == L"list")
		{
			std::cout << "All mapped storages on this device:" << std::endl;
			for (devDrvMap::iterator iter = myDrives->begin();
				iter != myDrives->end();
				iter++)
			{
				std::wcout << iter->first << std::endl;
			}
			std::cout << std::endl;
			std::cout << "-> Add new storages using the \"map\" command" << std::endl;
			std::cout << "-> Go to storage context using the \"storage\" command" << std::endl;
		}
		else if (cmd[0] == L"map")
		{
			std::vector<std::wstring> names;
			if(!conn->getMappableDriveNames(names, true) )
			{
				std::cout << "Drive names could not be obtained" << std::endl;
				continue;
			}
			if (names.empty())
			{
				std::cout << "No storage media found! Please unlock your device" << std::endl;
				continue;
			}

			std::map<int, int> userToActualIdx;
			int userIdxRun = 0;
			for (unsigned int i = 0; i < names.size(); i++)
			{
				try
				{
					myDrives->at(names.at(i));
				}
				catch (std::out_of_range)
				{
					std::wcout << "[" << userIdxRun << "]" << names.at(i) << std::endl;
					userToActualIdx.emplace(userIdxRun, i);
					userIdxRun++;
				}
			}
			if (userToActualIdx.empty())
			{
				std::cout << "All drives already mapped" << std::endl;
				continue;
			}
			else
			{
				int userIndexChoose = -1;
				std::cout << "Choose one by index: ";
				std::cin >> userIndexChoose;
				int actIdx;
				try
				{
					actIdx = userToActualIdx.at(userIndexChoose);
				}
				catch (std::out_of_range)
				{
					std::cout << "This is not a valid index" << std::endl;
					continue;
				}
				AbstractMappableDrive* newdrv = conn->getMDriveByFName(names.at(actIdx));
				if (newdrv == NULL)
				{
					std::cout << "Could not be mapped" << std::endl;
					continue;
				}
				myDrives->emplace(names.at(actIdx), newdrv);
				std::cout << "Mapping successful" << std::endl;
				storageMain(newdrv, names.at(actIdx));
			}
		}
		else if (cmd[0] == L"unmap" && cmd.size() == 2)
		{
			AbstractMappableDrive* toUnmap;
			try
			{
				toUnmap = myDrives->at(cmd[1]);
			}
			catch (std::out_of_range)
			{
				std::cout << "This is not a mapped drive" << std::endl;
				continue;
			}
			myDrives->erase(cmd[1]);
			std::wcout << "Successfully unmapped " << cmd[1] << std::endl;
		}
		else if (cmd[0] == L"storage" && cmd.size() == 2)
		{
			AbstractMappableDrive* drv;
			try
			{
				drv = myDrives->at(cmd[1]);
			}
			catch (std::out_of_range)
			{
				std::cout << "This is not a mapped drive" << std::endl;
				continue;
			}
			storageMain(drv, cmd[1]);
		}
		else
		{
			std::cout << "Command not found, or wrong parameters" << std::endl << std::endl;

			std::cout << "Available commands (device mode):" << std::endl;
			std::cout << "do <command>         execute an external program or command" << std::endl;
			std::cout << "exit                 return to global mode" << std::endl;
			std::cout << "list                 show all mapped drives" << std::endl;
			std::cout << "map                  map a new storage from the device" << std::endl;
			std::cout << "storage <drive-name> enter storage context" << std::endl;
			std::cout << "unmap <drive-name>   revoke a mapped drive" << std::endl << std::endl;
		}
	}

}

int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	AutoDeletePointer<MtpConnectionProvider> adp_mtpcp(&MtpConnectionProvider::getInstance());
	std::map<std::wstring, AbstractConnection*> connectedDevices;
	std::map<std::wstring, AbstractMappableDrive*> mappedDrives;

	bool didIDoAnything = true;

	while (1)
	{
		std::wstring input;
		std::vector<std::wstring> cmd;
		if (didIDoAnything) { std::wcout << "MtpAccess>"; }
		std::getline(std::wcin, input);
		Utils::tokenize(input, cmd);

		if (cmd.size() == 0)
		{
			didIDoAnything = false;
			continue;
		}
		didIDoAnything = true;
		if (cmd[0] == L"exit")
		{
			for (devMap::iterator iter = allDrives.begin();
				iter != allDrives.end();
				iter++)
			{
				delete iter->second;
			}
			std::wcout << L"Terminated by user" << std::endl;
			break;
		}

		else if (cmd[0] == L"device" && cmd.size() == 2)
		{
			AbstractConnection* conn;
			try
			{
				conn = connectedDevices.at(cmd[1]);
			}
			catch (std::out_of_range)
			{
				std::wcout << "Invalid device name given!" << std::endl;
				continue;
			}
			deviceMain(conn, cmd[1]);
			continue;
		}
		else if (cmd[0] == L"do")
		{
			std::string execCmd = Utils::wstringToString(input).substr(3);
			system(execCmd.c_str());
			continue;
		}

		else if (cmd[0] == L"connect")
		{
			MtpConnectionProvider::getInstance().refresh();
			int devcnt = MtpConnectionProvider::getInstance().getDeviceCount();

			if (devcnt < 1)
			{
				std::wcout << "No available devices" << std::endl;
				continue;
			}

			std::map<int, int> userToActualIdx;
			int userIdxRun = 0;
			for (int i = 0; i < devcnt; i++)
			{
				std::wstring str;
				if (MtpConnectionProvider::getInstance().getFriendlyNameOfDevice(i, str))
				{
					try
					{
						connectedDevices.at(str);
					}
					catch (std::out_of_range)
					{
						std::wcout << "[" << userIdxRun << "]" << str << std::endl;
						userToActualIdx.emplace(userIdxRun, i);
						userIdxRun++;
					}
				}
			}

			if (userToActualIdx.empty())
			{
				std::wcout << "All devices already connected!" << std::endl;
				continue;
			}
			else
			{
				std::wcout << "Select index: ";
				int userIdxIn = -1;
				std::wcin >> userIdxIn;
				int actualIdx = -1;
				try
				{
					actualIdx = userToActualIdx.at(userIdxIn);
				}
				catch (std::out_of_range)
				{
					std::wcout << "Index invalid" << std::endl;
					continue;
				}

				AbstractConnection* conn;
				bool success = MtpConnectionProvider::getInstance().openConnectionToDevice(actualIdx, &conn, false);

				if (success)
				{
					std::wcout << "Connecting succeeded" << std::endl;
					std::wstring name;
					MtpConnectionProvider::getInstance().getFriendlyNameOfDevice(actualIdx, name);
					connectedDevices.emplace( name, conn );
					deviceMain(conn, name);
				}
				else
				{
					std::wcout << "Connecting failed" << std::endl;
				}
				continue;
			}
		}
		else if (cmd[0] == L"list")
		{
			std::cout << "All connected devices:" << std::endl;
			for (std::map<std::wstring, AbstractConnection*>::iterator iter = connectedDevices.begin();
				iter != connectedDevices.end();
				iter++)
			{
				std::wcout << iter->first << std::endl;
			}
			std::cout << std::endl;
			std::cout << "-> Connect new devices using the \"connect\" command" << std::endl;
			std::cout << "-> Go to device context using the \"device\" command" << std::endl;
		}
		else if (cmd[0] == L"disconnect" && cmd.size() == 2)
		{
			AbstractConnection* disconnectMe = NULL;
			try
			{
				disconnectMe = connectedDevices.at(cmd[1]);
			}
			catch (std::out_of_range)
			{
				std::cout << "This device does not exist" << std::endl;
				continue;
			}
			MtpConnectionProvider::getInstance().closeConnection(disconnectMe);
			connectedDevices.erase(cmd[1]);
			try
			{
				delete allDrives.at(cmd[1]);
				allDrives.erase(cmd[1]);
			}
			catch(std::out_of_range) { }
			std::wcout << L"Device " << cmd[1] << L" disconnected" << std::endl;
			continue;
		}	
		else
		{
			std::wcout << "Command not found, or wrong parameters given" << std::endl << std::endl;

			std::wcout << "Available commands:" << std::endl;
			std::wcout << "connect                  connect a new device" << std::endl;
			std::wcout << "device <device-name>     open a device context" << std::endl;
			std::wcout << "disconnct <device-name>  disconnect a device" << std::endl;
			std::wcout << "do <command>             execute an external program or command" << std::endl;
			std::wcout << "exit                     terminate this program" << std::endl;
			std::wcout << "list                     list all connected devices" << std::endl << std::endl;
		}
	}
}