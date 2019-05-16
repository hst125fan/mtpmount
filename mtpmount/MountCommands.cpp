#include "..\dokanconnect\MemLeakDetection.h"
#include "MountCommands.h"
#include "ErrorReturnCodes.h"

ActiveConnection::ActiveConnection(AbstractConnection* conn) : _syncer(conn), _logger(NULL)
{
	_cache = new FileCache(_syncer);
	_backjob = new FileCacheBackgroundJob(*_cache, 5000, /*max dirty files*/5, /*max overall files*/50);
}

ActiveConnection::~ActiveConnection()
{
	unmapAllDrives();
	delete _backjob;
	delete _cache;
	if (_logger != NULL) { (*_logger) << "Cache OK" << std::endl; }
}

void ActiveConnection::setLogOstream(std::ostream* ostream)
{
	_logger = ostream;
	_syncer.setOutputter(ostream);
}

void ActiveConnection::unmapAllDrives()
{
	for (typeof_drivemap::iterator iter = _drivemap.begin(); iter != _drivemap.end(); iter++)
	{
		delete iter->second;
	}
	_drivemap.clear();
}

bool ActiveConnection::isDriveMapped(const std::wstring& drive)
{
	try
	{
		_drivemap.at(drive);
		return true;
	}
	catch (std::out_of_range)
	{
		return false;
	}
}

bool ActiveConnection::mapDrive(std::wstring& name, char drvLetter)
{
	try
	{
		_drivemap.at(name);
		return true;
	}
	catch (std::out_of_range)
	{
		DecideForGivenDriveName decider(name);
		int drvId = _syncer.mapNewDrive(decider);
		if (drvId < 0) { drvId = _syncer.mapNewDrive(decider, true); }
		if (drvId < 0) { return false; }

		try
		{
			DokanDriveWrapper* wrap = new DokanDriveWrapper(_syncer, *_cache, drvId, drvLetter, name);
			_drivemap.insert(typeof_drivemap::value_type(name, wrap));
			return true;
		}
		catch (DokanDriveWrapperMountFailedException)
		{
			return false;
		}
	}
}

void ActiveConnection::unmapDrive(std::wstring& name)
{
	try
	{
		delete _drivemap.at(name);
		_drivemap.erase(name);
	}
	catch (std::out_of_range)
	{
		//do nothing
	}
}

std::wstring ActiveConnection::driveLetterToName(char drvLetter)
{
	for (typeof_drivemap::iterator iter = _drivemap.begin(); iter != _drivemap.end(); iter++)
	{
		if (drvLetter == iter->second->getDriveLetter())
		{
			return iter->first;
		}
	}
	return L"";
}

void ActiveConnection::printMappings(std::ostream& stream, std::wstring devicename, std::string prefix)
{
	for (typeof_drivemap::iterator iter = _drivemap.begin(); iter != _drivemap.end(); iter++)
	{
		std::wstring name = iter->first;
		stream << prefix << Utils::wstringToString(name);
		stream << " [ID #" << MountAddressStore::getInstance().toStorageEnum(MountAddressStore::devStorageId(devicename, name)) << "]";
		stream << " (on " << iter->second->getDriveLetter() << ":\\)" << std::endl;
	}
}

std::string ActiveConnection::getDriveLetters()
{
	std::string back;
	for (typeof_drivemap::iterator iter = _drivemap.begin(); iter != _drivemap.end(); iter++)
	{
		back.append(1, iter->second->getDriveLetter());
	}
	return back;
}


MountAddressStore& MountAddressStore::getInstance()
{
	if (_instance == NULL)
	{
		_instance = new MountAddressStore();
	}
	return *_instance;
}

ActiveConnection* MountAddressStore::openConnection(const std::wstring& devicename)
{
	try
	{
		return _connectionmap.at(devicename);
	}
	catch (std::out_of_range)
	{
		MtpConnectionProvider::getInstance().refresh();
		AbstractConnection* conn = NULL;
		for (size_t i = 0; i < MtpConnectionProvider::getInstance().getDeviceCount(); i++)
		{
			std::wstring name;
			MtpConnectionProvider::getInstance().getFriendlyNameOfDevice(i, name);
			if (name == devicename)
			{
				MtpConnectionProvider::getInstance().openConnectionToDevice(i, &conn, false);
				break;
			}
		}

		if (conn != NULL)
		{
			ActiveConnection* newactiveconn = new ActiveConnection(conn);
			_connectionmap.insert(typeof_connectionmap::value_type(devicename, newactiveconn));
#ifdef _DEBUG
			newactiveconn->setLogOstream(&std::cout);
#endif
			return newactiveconn;
		}
		else
		{
			return NULL;
		}
	}
}

void MountAddressStore::closeConnection(const std::wstring& devicename, std::ostream* log)
{
	try
	{
		_connectionmap.at(devicename)->setLogOstream(log);
		delete _connectionmap.at(devicename);
		_connectionmap.erase(devicename);
	}
	catch (std::out_of_range)
	{
		//do nothing
	}
}

bool MountAddressStore::isConnectionOpen(const std::wstring& devicename)
{
	try
	{
		_connectionmap.at(devicename);
		return true;
	}
	catch (std::out_of_range)
	{
		return false;
	}
}

void MountAddressStore::closeAllConnections()
{
	for (typeof_connectionmap::iterator iter = _connectionmap.begin(); iter != _connectionmap.end(); iter++)
	{
		delete iter->second;
	}
	_connectionmap.clear();
}

ActiveConnection* MountAddressStore::whoHasThisDriveLetter(char letter)
{
	for (typeof_connectionmap::iterator iter = _connectionmap.begin(); iter != _connectionmap.end(); iter++)
	{
		std::string drvLetters = iter->second->getDriveLetters();
		if (drvLetters.find(letter) != std::string::npos)
		{
			return iter->second;
		}
	}
	return NULL;
}

std::wstring MountAddressStore::whoHasThisDriveLetterName(char letter)
{
	for (typeof_connectionmap::iterator iter = _connectionmap.begin(); iter != _connectionmap.end(); iter++)
	{
		std::string drvLetters = iter->second->getDriveLetters();
		if (drvLetters.find(letter) != std::string::npos)
		{
			return iter->first;
		}
	}
	return L"";
}

void MountAddressStore::printAllActive(std::ostream& stream)
{
	for (typeof_connectionmap::iterator iter = _connectionmap.begin(); iter != _connectionmap.end(); iter++)
	{
		std::wstring devicename = iter->first;
		stream << "\tDevice " << Utils::wstringToString(devicename) << ":" << std::endl;
		iter->second->printMappings(stream, devicename, "\t |-- Storage " );
	}
}

void MountAddressStore::printAllAvailable(std::ostream& stream)
{
	MtpConnectionProvider::getInstance().refresh();
	int devcnt = MtpConnectionProvider::getInstance().getDeviceCount();
	if (devcnt < 1)
	{
		stream << "There are currently no MTP devices connected to this PC!" << std::endl;
		return;
	}
	for (int i = 0; i < devcnt; i++)
	{
		std::wstring name;
		MtpConnectionProvider::getInstance().getFriendlyNameOfDevice(i, name);
		stream << "Connection " << Utils::wstringToString(name) << ": ";
		if (MountAddressStore::getInstance().isConnectionOpen(name))
		{
			stream << "is currently open" << std::endl;
			ActiveConnection* aconn = MountAddressStore::getInstance().openConnection(name);
			AbstractConnection* conn = NULL;
			bool canConnect = MtpConnectionProvider::getInstance().openConnectionToDevice(i, &conn, true);
			if (!canConnect || conn == NULL)
			{
				stream << "Cannot open connection to already connected device!" << std::endl;
				continue;
			}
			std::vector<std::wstring> names;
			conn->getMappableDriveNames(names);
			MtpConnectionProvider::getInstance().closeConnection(conn);
			if (names.size() == 0)
			{
				stream << "Device does not contain any drives. Therefore no connection should be open." << std::endl;
				continue;
			}
			for (std::vector<std::wstring>::iterator iter = names.begin(); iter != names.end(); iter++)
			{
				stream << " |-- Storage " << Utils::wstringToString(*iter) << " [ID #" << toStorageEnum(devStorageId(name,*iter)) << "]";
				if (aconn->isDriveMapped(*iter))
				{
					stream << " (is mounted)" << std::endl;
				}
				else
				{
					stream << " (is not mounted)" << std::endl;
				}
			}
		}
		else
		{
			AbstractConnection* conn = NULL;
			bool canConnect = MtpConnectionProvider::getInstance().openConnectionToDevice(i, &conn, true);
			if (!canConnect || conn == NULL)
			{
				stream << "Cannot connect to device" << std::endl;
				continue;
			}
			std::vector<std::wstring> names;
			conn->getMappableDriveNames(names);
			MtpConnectionProvider::getInstance().closeConnection(conn);
			if (names.size() == 0)
			{
				stream << "Contains no drives or is locked" << std::endl;
				continue;
			}
			stream << "Contains " << names.size() << " storages that can be mounted" << std::endl;
			for (std::vector<std::wstring>::iterator iter = names.begin(); iter != names.end(); iter++)
			{
				stream << " |-- Storage " << Utils::wstringToString(*iter) << " [ID #" << toStorageEnum(devStorageId(name, *iter)) << "] " << std::endl;
			}

		}
	}
}

MountAddressStore::devStorageEnum MountAddressStore::toStorageEnum(const MountAddressStore::devStorageId& id)
{
	try
	{
		return _idset.at(id);
	}
	catch (std::out_of_range)
	{
		size_t nextId = _idvector.size();
		std::pair<typeof_idset::iterator,bool> insertres = _idset.insert(typeof_idset::value_type(id, nextId));
		const devStorageId* idptr = &(insertres.first->first);
		_idvector.push_back(const_cast<devStorageId*>(idptr));
		return nextId;
	}
}

MountAddressStore::devStorageId MountAddressStore::toStorageId(MountAddressStore::devStorageEnum enm)
{
	if (enm >= _idvector.size())
	{
		return devStorageId(L"", L"");
	}
	else
	{
		return *(_idvector.at(enm));
	}
}

bool MountAddressStore::decideIdParameter(const std::string& parameter, std::wstring& devicename, std::wstring& storagename)
{
	if (parameter.size() < 2 || parameter.at(0) != '#')
	{
		return false;
	}
	unsigned long enm;
	try
	{
		 enm = std::stoul(parameter.substr(1, std::string::npos));
	}
	catch (std::invalid_argument)
	{
		return false;
	}
	devStorageId id = toStorageId(enm);
	if (id.connection == L"" && id.storage == L"")
	{
		return false;
	}
	devicename = id.connection;
	storagename = id.storage;
	return true;
}

MountAddressStore* MountAddressStore::_instance = NULL;

MountSubCommand::MountSubCommand() : AbstractSubCommand("mount")
{
	
}

const char* MountSubCommand::getBriefDescription()
{
	return "mount MTP-connected storage media as removable drive";
}

const char* MountSubCommand::getFullDescription()
{
	return "mount MTP-connected storage media as removable drive\n"
		"Usage: xEXECNAMEx mount <devicename> <storagename> (<driveletter>)\n"
		"or xEXECNAMEx mount #<device-id> (<driveletter>)\n"
		"obtain available names (or ID) using xEXECNAMEx list available";
}

int MountSubCommand::executeCommand(SubParameters& subparams, std::ostream& output)
{
	std::wstring device;
	std::wstring drive;
	char drvLetter;
	if (subparams.size() > 0 && MountAddressStore::getInstance().decideIdParameter(subparams.at(0), device, drive))
	{
		drvLetter = (subparams.size() > 1 && subparams.at(1).size() > 0) ? subparams.at(1).at(0) : 0;
	}
	else
	{
		device = (subparams.size() > 0) ? Utils::stringToWstring(subparams.at(0)) : L"";
		drive = (subparams.size() > 1) ? Utils::stringToWstring(subparams.at(1)) : L"";
		drvLetter = (subparams.size() > 2 && subparams.at(2).size() > 0) ? subparams.at(2).at(0) : 0;
	}

	if (drvLetter > 'Z') drvLetter -= ('a' - 'A');

	if (drvLetter == 0)
	{
		std::string available = Utils::getAvailableDrives();
		if (available.empty())
		{
			output << "There are no free drive letters available! Mounting not possible." << std::endl;
			return MTPMOUNT_NO_MORE_DRIVE_LETTERS;
		}
		drvLetter = available.at(available.size() - 1);
		if (drvLetter > 'Z') drvLetter -= ('a' - 'A');
	}
	else if (drvLetter < 'A' || drvLetter > 'Z')
	{
		output << drvLetter << " cannot be used drive letter!" << std::endl;
		return MTPMOUNT_NOT_A_DRIVE_LETTER;
	}
	else if (Utils::getAvailableDrives().find(drvLetter) == std::string::npos)
	{
		output << drvLetter << ":\\ is already used. Run the same command without the drive letter parameter to use an unused drive!" << std::endl;
		return MTPMOUNT_DRIVE_LETTER_OCCUPIED;
	}

	if (device == L"" || drive == L"")
	{
		output << "Usage: mount <devicename> <storagename> (<driveletter>)" << std::endl;
		return MTPMOUNT_WRONG_PARAMETRIZATION;
	}

	ActiveConnection* ac = MountAddressStore::getInstance().openConnection(device);
	if (ac == NULL)
	{
		output << "Cannot establish connection to device. Is " << Utils::wstringToString(device) << " really connected (and spelled correctly)?" << std::endl;
		return MTPMOUNT_CONNECTION_FAIL;
	}

	if (ac->mapDrive(drive, drvLetter))
	{

		output << "Drive " << drvLetter << ":\\ is now " << Utils::wstringToString(device) << "\\" << Utils::wstringToString(drive) << ". Don't forget to unmount the drive (using unmount command) before disconnecting your device" << std::endl;
		return MTPMOUNT_RETURN_SUCESS;
	}
	else
	{
		output << "Did not find any storage media named " << Utils::wstringToString(drive) << " on " << Utils::wstringToString(device) << ". This might be due to wrong spelling or your device not being unlocked." << std::endl;
		if (ac->getDriveLetters() == "") { MountAddressStore::getInstance().closeConnection(device); }
		return MTPMOUNT_STORAGE_UNAVAILABLE;
	}
}

UnmountSubCommand::UnmountSubCommand() : AbstractSubCommand("unmount")
{

}

const char* UnmountSubCommand::getBriefDescription()
{
	return "unmount drives/storage media mounted by this program";
}

const char* UnmountSubCommand::getFullDescription()
{
	return "unmount drives/storage media mounted by this program\n"
		"Usage: xEXECNAMEx unmount <devicename> <storagename> or unmount <driveletter>\n"
		"or xEXECNAMEx unmount #<device-id>\n"
		"obtain available names, driveletters or IDs using xEXECNAMEx list active";
}

int UnmountSubCommand::executeCommand(SubParameters& subparams, std::ostream& output)
{
	std::wstring device = L"";
	std::wstring drive = L"";
	if (subparams.size() == 1 && MountAddressStore::getInstance().decideIdParameter(subparams.at(0), device, drive))
	{
		//everything done!
	}
	else if (subparams.size() == 1 && subparams.at(0).size() > 0)
	{
		char drvLetter = subparams.at(0).at(0);
		if (drvLetter > 'Z') drvLetter -= ('a' - 'A');
		ActiveConnection* conn = MountAddressStore::getInstance().whoHasThisDriveLetter(drvLetter);
		if (conn == NULL)
		{
			output << "Drive " << subparams.at(0).at(0) << ":\\ is currently not in use by this application" << std::endl;
			return MTPMOUNT_DRIVE_NOT_MAPPED;
		}
		device = MountAddressStore::getInstance().whoHasThisDriveLetterName(drvLetter);
		drive = conn->driveLetterToName(drvLetter);
		
	}
	else
	{
		device = (subparams.size() > 0) ? Utils::stringToWstring(subparams.at(0)) : L"";
		drive = (subparams.size() > 1) ? Utils::stringToWstring(subparams.at(1)) : L"";
	}

	if (device == L"" || drive == L"")
	{
		output << "Usage: unmount <devicename> <storagename> or unmount <driveletter>" << std::endl;
		return MTPMOUNT_WRONG_PARAMETRIZATION;
	}

	if (!MountAddressStore::getInstance().isConnectionOpen(device))
	{
		output << Utils::wstringToString(device) << " does not refer to an open connection. Check for spelling!" << std::endl;
		return MTPMOUNT_CONNECTION_NOT_OPEN;
	}

	ActiveConnection* ac = MountAddressStore::getInstance().openConnection(device);

	if (!ac->isDriveMapped(drive))
	{
		output << Utils::wstringToString(device) << "\\" << Utils::wstringToString(drive) << " is not mapped. Did you mean..." << std::endl;
		ac->printMappings(output, device, "\t");
		return MTPMOUNT_DRIVE_NOT_MAPPED;
	}

	ac->unmapDrive(drive);
	output << Utils::wstringToString(device) << "\\" << Utils::wstringToString(drive) << " has been unmounted successfully." << std::endl;
	if (ac->getDriveLetters() == "")
	{
		output << "Syncing " << Utils::wstringToString(device) << ". DO NOT UNPLUG THIS DEVICE YET!" << std::endl;
		MountAddressStore::getInstance().closeConnection(device, &output);
		output << "All content synced to " << Utils::wstringToString(device) << ". You may now unplug this device." << std::endl;
	}
	else
	{
		output << "Please mind that " << Utils::wstringToString(device) << " is still in use by other drive mappings" << std::endl;
	}
	return MTPMOUNT_RETURN_SUCESS;
}


TerminateConnectionSubCommand::TerminateConnectionSubCommand() : AbstractSubCommand("unplug")
{

}

const char* TerminateConnectionSubCommand::getBriefDescription()
{
	return "disconnects a connected device to make it ready to be unplugged";
}

const char* TerminateConnectionSubCommand::getFullDescription()
{
	return "This command removes all mapped drives which are located on a given device\n"
		"and ensures all changed content has been synced to the device.\n"
		"Use this command before unplugging a device.\n"
		"Usage: xEXECNAMEx unplug <devicename>";
}

int TerminateConnectionSubCommand::executeCommand(SubParameters& subparams, std::ostream& output)
{
	if (subparams.size() != 1)
	{
		output << "Usage: unplug <devicename>" << std::endl;
		return MTPMOUNT_WRONG_PARAMETRIZATION;
	}

	std::wstring device = Utils::stringToWstring(subparams.at(0));
	if (!MountAddressStore::getInstance().isConnectionOpen(device))
	{
		output << Utils::wstringToString(device) << " does not refer to an open connection. Check for spelling!" << std::endl;
		return MTPMOUNT_CONNECTION_NOT_OPEN;
	}

	ActiveConnection* ac = MountAddressStore::getInstance().openConnection(device);
	ac->unmapAllDrives();
	output << "Syncing " << Utils::wstringToString(device) << ". DO NOT UNPLUG THIS DEVICE YET!" << std::endl;
	MountAddressStore::getInstance().closeConnection(device, &output);
	output << "All content synced to " << Utils::wstringToString(device) << ". You may now unplug this device." << std::endl;
	return MTPMOUNT_RETURN_SUCESS;
}

ListDrivesSubCommand::ListDrivesSubCommand() : AbstractSubCommand("list")
{

}

const char* ListDrivesSubCommand::getBriefDescription()
{
	return "lists all drives mounted (active) or mountable (available) by this program";
}

int ListDrivesSubCommand::executeCommand(SubParameters& subparams, std::ostream& output)
{
	if (subparams.size() == 1 && subparams.at(0) == "active")
	{
		output << "Active Connections and Mappings:" << std::endl;
		MountAddressStore::getInstance().printAllActive(output);
		output << "Use the unmount or unplug command to remove mappings or terminate connections" << std::endl;
		return MTPMOUNT_RETURN_SUCESS;
	}
	else if(subparams.size() == 1 && subparams.at(0) == "available")
	{
		output << "Available Connections and Storage Media:" << std::endl;
		MountAddressStore::getInstance().printAllAvailable(output);
		output << "Use mount command to make one of them a windows removable drive" << std::endl;
		return MTPMOUNT_RETURN_SUCESS;
	}
	else
	{
		output << "Usage: list (active|available)" << std::endl;
		return MTPMOUNT_WRONG_PARAMETRIZATION;
	}
}

bool operator<(const MountAddressStore::devStorageId& a, const MountAddressStore::devStorageId& b)
{
	return(std::wstring(a.connection + L"\\" + a.storage) < std::wstring(b.connection + L"\\" + b.storage));
}


defActivateSubCommand(MountSubCommand)
defActivateSubCommand(UnmountSubCommand)
defActivateSubCommand(TerminateConnectionSubCommand)
defActivateSubCommand(ListDrivesSubCommand)