#include "..\dokanconnect\MemLeakDetection.h"
#include "SubCommandLibrary.h"
#include "IPC.h"
#include "..\mtpaccess\Utils.h"
#include "..\mtpaccess\ConnectionProvider.h"
#include "ErrorReturnCodes.h"
#include "MountCommands.h"


int daemonMain(std::vector<std::string>& commands, IPCStringOutputter& strout)
{
	SubCommandLibrary::getInstance().setExecName(commands.at(0));
	std::string subcmd = (commands.size() > 1) ? commands.at(1) : "cmdlist";
	SubParameters subparams;
	for (size_t i = 2; i < commands.size(); i++)
	{
		subparams.push_back(commands.at(i));
	}

	return SubCommandLibrary::getInstance().executeCommand(subcmd, subparams, strout.queryOutStream());
}

void ensureDaemonIsRunning(std::string execName)
{
	if (!IPCEndPoint::doesEndpointExist("mtpmountdaemon"))
	{
		STARTUPINFOA stupinfo;
		PROCESS_INFORMATION procinfo;
		ZeroMemory(&stupinfo, sizeof(STARTUPINFO));
		ZeroMemory(&procinfo, sizeof(PROCESS_INFORMATION));
		stupinfo.cb = sizeof(STARTUPINFO);
		std::string runName("\"" + execName + "\" daemon");
		IPCEndPoint startupListener("mtpmountstartup", 1024, 1000);
		if(!CreateProcessA(NULL, const_cast<char*>(runName.c_str()), NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &stupinfo, &procinfo))
		{
			std::cout << "Not able to create process" << std::endl;
			std::exit(MTPMOUNT_PROCESS_CREATION_FAILED);
		}
		std::string startupcommand = startupListener.recvString();
		if (startupcommand != "mtpmountstartup-ok")
		{
			std::cout << "Startup of mtpmount-daemon failed!" << std::endl;
			std::exit(MTPMOUNT_CREATED_PROCESS_NO_RESPONSE);
		}
	}
}

int main(int argc, char**argv)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	AutoDeletePointer<SubCommandLibrary> autoDeleteScl = &(SubCommandLibrary::getInstance());
	if (argc == 2 && std::string(argv[1]) == std::string("daemon"))
	{
		AutoDeletePointer<MtpConnectionProvider> autoDeleteMcp = &(MtpConnectionProvider::getInstance());
		AutoDeletePointer<CacheLocationProvider> autoDeleteClp = &(CacheLocationProvider::getInstance());
		AutoDeletePointer<MountAddressStore> autoDeleteMas = &(MountAddressStore::getInstance());
		std::cout << "===MTPMOUNTDAEMON===" << std::endl;
		IPCForDaemon ipc;
		ipc.signalStartup();
		ipc.listen(daemonMain);
		return 0;
	}
	else
	{
		if (argc == 3 && std::string(argv[1]) == std::string("daemonctl"))
		{
			//special local commands
			if (!IPCEndPoint::doesEndpointExist("mtpmountdaemon")) { return MTPMOUNT_DAEMONCTL_NO_DAEMON; }
			if (std::string(argv[2]) == std::string("terminate"))
			{
				IPCForController ipc;
				return ipc.command("x", std::cout);
			}
			if (std::string(argv[2]) == std::string("restart"))
			{
				IPCForController ipc;
				ipc.command("x", std::cout);
				std::wstring execPath = Utils::getExecutablePath();
				ensureDaemonIsRunning(Utils::wstringToString(execPath));
				return MTPMOUNT_RETURN_SUCESS;
			}
		}
		std::wstring execPath = Utils::getExecutablePath();
		ensureDaemonIsRunning(Utils::wstringToString(execPath));
		IPCForController ipc;
		std::string sendcommand("c");
		bool first = true;
		for (int i = 0; i < argc; i++)
		{
			if (first) { first = false; }
			else { sendcommand.append(" "); }
			sendcommand.append("\"");
			sendcommand.append(argv[i]);
			sendcommand.append("\"");
		}
		return ipc.command(sendcommand, std::cout);
	}
}