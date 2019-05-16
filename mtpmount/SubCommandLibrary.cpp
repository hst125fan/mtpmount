#include "..\dokanconnect\MemLeakDetection.h"
#include "SubCommandLibrary.h"
#include <iostream>
#include "ErrorReturnCodes.h"
#include "gitinfo.h"

AbstractSubCommand::AbstractSubCommand(const std::string& cmdname) :  _cmdname(cmdname)
{
	SubCommandLibrary::getInstance().addSubCommand(*this);
}

AbstractSubCommand::~AbstractSubCommand()
{
	SubCommandLibrary::getInstance().removeSubCommand(*this);
}

SubCommandLibrary::SubCommandLibrary()
{
	_cmdWidthCounter = 0;
	_execname = "mtpmount";
}

SubCommandLibrary& SubCommandLibrary::getInstance()
{
	if (_instance == nullptr)
	{
		_instance = new SubCommandLibrary;
	}
	return *_instance;
}

void SubCommandLibrary::addSubCommand(AbstractSubCommand& cmd)
{
	std::string cmdname( cmd.getCommandName() );
	_subcommands.insert(std::map<std::string, AbstractSubCommand*>::value_type(cmdname, &cmd));
	int newsize = std::string(cmd.getCommandName()).size();
	_cmdWidthCounter = (newsize > _cmdWidthCounter) ? newsize : _cmdWidthCounter;
}

void SubCommandLibrary::removeSubCommand(AbstractSubCommand& cmd)
{
	_subcommands.erase(cmd.getCommandName());
}

void SubCommandLibrary::getCommandList(std::ostream& output)
{
	output << "All available commands:" << std::endl;
	for (std::map<std::string, AbstractSubCommand*>::iterator iter = _subcommands.begin(); iter != _subcommands.end(); iter++)
	{
		output << iter->second->getCommandName();
		for (int i = std::string(iter->second->getCommandName()).size(); i < _cmdWidthCounter; i++)
		{
			output << " ";
		}
		output << "\t" << iter->second->getBriefDescription() << std::endl;
	}
	output << std::endl;
	output << "Use "<< _execname << " help <command> to get a more detailed description of a specific subcommand" << std::endl;
}

bool SubCommandLibrary::getCommandHelp(const std::string& cmd, std::ostream& output)
{
	AbstractSubCommand* myCmd = NULL;
	try
	{
		myCmd = _subcommands.at(cmd);
	}
	catch (std::out_of_range)
	{
		return false;
	}
	output << "=== Command " << myCmd->getCommandName() << " ===" << std::endl;
	std::string desc(myCmd->getFullDescription());
	size_t offset = 0;
	while((offset = desc.find("xEXECNAMEx",offset)) != std::string::npos)
	{
		desc.replace(offset, 10, _execname);
		offset += _execname.length();
	}
	output << desc << std::endl;
	return true;
}

int SubCommandLibrary::executeCommand(const std::string& cmd, SubParameters& params, std::ostream& output)
{
	if (std::string(getGitTag()) == std::string("untagged"))
	{
		output << "This is an experimental build of mtpmount at " << getGitCommit() << std::endl;
	}
	else
	{
		output << "This is mtpmount, version " << getGitTag() << " from commit " << getGitCommit() << std::endl;
	}
	output << "This program comes with NO WARRANTY. Usage only at your own risk." << std::endl << std::endl;

	try
	{
		return _subcommands.at(cmd)->executeCommand(params, output);
	}
	catch (std::out_of_range)
	{
		output << "Command " << cmd << " was not found!" << std::endl;
		output << "Run " << _execname << " cmdlist to get a list of all commands" << std::endl;
		return MTPMOUNT_SUBCOMMAND_DOES_NOT_EXIST;
	}
}

SubCommandLibrary::~SubCommandLibrary()
{
	std::map<std::string, AbstractSubCommand*> tmp = _subcommands;
	for (std::map<std::string, AbstractSubCommand*>::iterator iter = tmp.begin(); iter != tmp.end(); iter++)
	{
		delete iter->second;
	}
}

int HelpCommand::executeCommand(SubParameters& subparams, std::ostream& output)
{
	if (subparams.size() != 1)
	{
		output << "Usage: " << SubCommandLibrary::getInstance().getExecName() << " help <command>" << std::endl;
		return MTPMOUNT_WRONG_PARAMETRIZATION;
	}
	return ( SubCommandLibrary::getInstance().getCommandHelp(subparams.at(0), output) ) ? MTPMOUNT_RETURN_SUCESS : MTPMOUNT_HELP_INVALID_SUBCOMMAND;
}

int CmdListCommand::executeCommand(SubParameters& subparams, std::ostream& output)
{
	SubCommandLibrary::getInstance().getCommandList(output);
	return MTPMOUNT_RETURN_SUCESS;
}

SubCommandLibrary* SubCommandLibrary::_instance = NULL;

defActivateSubCommand( HelpCommand )
defActivateSubCommand( CmdListCommand )