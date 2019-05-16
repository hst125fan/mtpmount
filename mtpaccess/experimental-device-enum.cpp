#include "MtpTransfer.h"
#include "Utils.h"
#include <iostream>
#include <string>

#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>


int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	int devcnt = MtpConnectionProvider::getInstance().getDeviceCount();

	AutoDeletePointer<MtpConnectionProvider> adp_mtpcp(&MtpConnectionProvider::getInstance());

	std::cout << "Devices connected: " << devcnt << std::endl;
	if (devcnt == 0) {
		system("pause");
		return 0;
	}
	for (int i = 0; i < devcnt; i++)
	{
		std::wstring str;
		if (MtpConnectionProvider::getInstance().getFriendlyNameOfDevice(i, str))
		{
			std::cout << "Device #" << i << " is " << Utils::wstringToString(str) << std::endl;
		}
		else
		{
			std::cout << "No information got for device #" << i << std::endl;
		}
	}
	int index;
	std::cout << "To which device should I connect? ";
	std::cin >> index;
	MtpConnection* myConn;
	bool ret = MtpConnectionProvider::getInstance().openConnectionToDevice(index, &myConn, false);
	if (!ret) 
	{ 
		std::cout << "Connection opening failed!" << std::endl;
		system("pause");
	}
	
	std::cout << "Connection successfully opened!" << std::endl;

	int drivecnt = 0;
	while (drivecnt == 0)
	{
		std::vector<std::wstring> drivenames;
		myConn->getMappableDriveCnt(drivecnt, true);
		bool test = myConn->getMappableDriveNames(drivenames);
		std::cout << "Found " << drivecnt << " mappable drives:" << std::endl;
		int counter = 0;
		for (std::vector<std::wstring>::iterator iter = drivenames.begin();
			iter != drivenames.end();
			iter++)
		{
			std::cout << counter << " " << Utils::wstringToString(*iter) << std::endl;
			counter++;
		}
		if (drivecnt == 0) { continue; }
		std::cout << "Choose one: ";
		int drivechoose;
		std::cin >> drivechoose;
		MtpMappableDrive* mapdrive = myConn->getMDriveByFName(drivenames.at(drivechoose));

		/*FsPath music(L"Musik/Chelsea Cutler - The Reason.mp3");
		std::ofstream out("C:/Users/PC-Nutzer/Desktop/test.mp3", std::ios::binary);
		mapdrive->readFile(music, out);*/

		/*std::wstring filesizeStr(L"C:/Users/PC-Nutzer/Desktop/test.mp3");
		std::ifstream in("C:/Users/PC-Nutzer/Desktop/test.mp3", std::ios::binary);
		unsigned long long filesize = Utils::getOSFileSize(filesizeStr);
		FsPath transf(L"bluetooth");
		bool check = mapdrive->createFileWithStream(transf, L"test.mp3", in, filesize);*/

		//FsPath deletepath(L"deleteme");
		//bool check = mapdrive->deleteFileOrDirectory(deletepath, false);

		FsPath newFolder(L"Transfer");
		bool check = mapdrive->createFolder(newFolder, L"whatthefuck");

		/*FsPath here(L"transfertest.txt");
		std::ifstream from("C:/MTPTEST.txt");
		bool exists = mapdrive->checkIfExists(here);
		bool written = mapdrive->writeFile(here, from);
		std::cout << ((exists) ? "exists" : "does not exist") << ((written) ? "OK" : "NOK") << std::endl;*/
		/*MtpFsNode*  mapdrivefs = mapdrive->getRawFs();

		std::vector<MtpFsNode*> rootlayercontent;
		mapdrivefs->getDirContent(rootlayercontent);

		for (std::vector<MtpFsNode*>::iterator iter = rootlayercontent.begin();
			iter != rootlayercontent.end();
			iter++)
		{
			std::wstring tmp = (*iter)->getNodeName();
			std::cout << ((*iter)->isDirectory() ? "D" : "F") << " " << Utils::wstringToString(tmp) << std::endl;
		}*/
		system("pause");
	}
	MtpConnectionProvider::getInstance().closeConnection(myConn);
}