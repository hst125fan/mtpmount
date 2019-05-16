#pragma once
#include <string>
#include <Windows.h>
#include <sstream>
#include <iostream>
#include <vector>

class IPCEndPoint
{
private:
	HANDLE _hSlot;
	int _size;
public:
	IPCEndPoint(const std::string& name, int size, DWORD waitTime = MAILSLOT_WAIT_FOREVER);
	~IPCEndPoint();
	bool sendString(const std::string& str, const std::string& destination);
	std::string recvString();
	static bool doesEndpointExist(const std::string& name);
};

class IPCForDaemon;

class IPCStringOutputter : public std::stringbuf
{
private:
	IPCForDaemon* _creator;
	IPCStringOutputter(IPCForDaemon& creator) : _creator(&creator) { _ostream = NULL; }
	~IPCStringOutputter() { delete _ostream; }
	friend class IPCForDaemon;
	std::ostream* _ostream;
public:
	virtual int sync(); 
	std::ostream& queryOutStream() { if (_ostream == NULL) { _ostream = new std::ostream(this); } return *_ostream; }
};

class IPCForDaemon
{
private:
	IPCEndPoint _ep;
	void _stringout(const std::string& out);
	friend class IPCStringOutputter;
public:
	IPCForDaemon();
	void listen(int (*callback)(std::vector<std::string>&,IPCStringOutputter&));
	void signalStartup() { _ep.sendString("mtpmountstartup-ok", "mtpmountstartup"); }
};

class IPCForController
{
private:
	IPCEndPoint _ep;
public:
	IPCForController() : _ep("mtpmountcontroller", 1024) { }
	int command(const std::string& cmd, std::ostream& outputter);
};



