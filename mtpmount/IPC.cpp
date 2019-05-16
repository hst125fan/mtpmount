#include "..\dokanconnect\MemLeakDetection.h"
#include "IPC.h"
#include "..\mtpaccess\Utils.h"

IPCEndPoint::IPCEndPoint(const std::string& name, int size, DWORD waittime) : _size(size)
{
	std::string actualName("\\\\.\\mailslot\\" + name);
	_hSlot = CreateMailslotA(actualName.c_str(), _size, waittime, NULL);
	if (_hSlot == INVALID_HANDLE_VALUE)
	{
		throw std::exception("Mailslot creation unsccessful");
	}
}

IPCEndPoint::~IPCEndPoint()
{
	CloseHandle(_hSlot);
}

bool IPCEndPoint::doesEndpointExist(const std::string& name)
{
	std::string actualName("\\\\.\\mailslot\\" + name);
	HANDLE hSlot = CreateMailslotA(actualName.c_str(), 1024, 100, NULL);
	if (hSlot == INVALID_HANDLE_VALUE) { return true; }
	
	CloseHandle(hSlot);
	return false;
}

std::string IPCEndPoint::recvString()
{
	char* buffer = new char[_size + 1];
	memset(buffer, 0, _size + 1);
	BOOL success = FALSE;
	DWORD actual = 0;
	while (!success || actual == 0)
	{
		success = ReadFile(_hSlot, buffer, _size, &actual, NULL);
	}
	std::string ret(buffer);
	delete buffer;
	return ret;
}

bool IPCEndPoint::sendString(const std::string& str, const std::string& destination)
{
	std::string actualDest("\\\\.\\mailslot\\" + destination);
	HANDLE hFile = CreateFileA(actualDest.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD written = 0;
	BOOL result = WriteFile(hFile, str.c_str(), str.length(), &written, NULL);
	CloseHandle(hFile);
	if (written != str.length() || !result)
	{
		return false;
	}
	return true;
}

IPCForDaemon::IPCForDaemon() : _ep( "mtpmountdaemon", 1024 )
{

}

void IPCForDaemon::listen(int(*callback)(std::vector<std::string>&, IPCStringOutputter&))
{
	while (1)
	{
		std::string str(_ep.recvString());
		if (str[0] == 'x') 
		{ 
			//exitcommand
			_ep.sendString("r0", "mtpmountcontroller");
			break; 
		} 

		else if (str[0] == 'c')
		{
			std::string command = str.substr(1, std::string::npos);
			std::vector<std::string> cmdvec;
			Utils::tokenize(command, cmdvec);
			IPCStringOutputter strout(*this);
			int ret = callback(cmdvec,strout);
			_ep.sendString("r" + std::to_string((long long)ret), "mtpmountcontroller");
		}


	}
}

void IPCForDaemon::_stringout(const std::string& out)
{
	_ep.sendString("o" + out, "mtpmountcontroller");
}

int IPCStringOutputter::sync()
{ 
	std::string s = this->str();
	_creator->_stringout(s); 
	this->_Tidy(); //?
	return s.length();
}

int IPCForController::command(const std::string& cmd, std::ostream& outputter)
{
	_ep.sendString(cmd, "mtpmountdaemon");
	std::string str = "n";
	while (str[0] != 'r')
	{
		if (str[0] == 'o')
		{
			outputter << str.substr(1, std::string::npos);
		}
		str = _ep.recvString();
	}
	int returncode = std::stoi(str.substr(1, std::string::npos));
	return returncode;
}