#pragma once
#include <string>
#include <vector>
#include <map>

typedef std::vector<std::string> SubParameters;

#define defActivateSubCommand( commandClass ) commandClass##* commandClass##Instance = new commandClass;


class AbstractSubCommand
{
private:
	std::string _cmdname;
public:
	AbstractSubCommand(const std::string& cmdname);
	/*virtual*/ ~AbstractSubCommand();
	const char* getCommandName() { return _cmdname.c_str(); }
	virtual const char* getBriefDescription() = 0;
	virtual const char* getFullDescription() { return getBriefDescription(); }
	virtual int executeCommand(SubParameters& subparams, std::ostream& output) = 0;
};

class HelpCommand : public AbstractSubCommand
{
public:
	HelpCommand() : AbstractSubCommand("help") { }
	virtual const char* getBriefDescription() { return "shows detailed information and usage for a specific subcommand"; }
	virtual const char* getFullDescription() {
		return "shows detailed information and usage for a specific subcommand.\n"
			"use: xEXECNAMEx help <subcommand>\n"
			"run mtpmount cmdlist to get a list of all available subcommands";
	}
	virtual int executeCommand(SubParameters& subparams, std::ostream& output);
};

class CmdListCommand : public AbstractSubCommand
{
public:
	CmdListCommand() : AbstractSubCommand("cmdlist") { }
	virtual const char* getBriefDescription() { return "Does the thing that is currently happening"; }
	virtual const char* getFullDescription() { return "Shows a list of all available subcommands"; }
	virtual int executeCommand(SubParameters& subparams, std::ostream& output);
};


class SubCommandLibrary
{
private:
	SubCommandLibrary();
	static SubCommandLibrary* _instance;
	SubCommandLibrary(const SubCommandLibrary& lib) = delete;
	SubCommandLibrary& operator=(const SubCommandLibrary& lib) = delete;

	std::map<std::string, AbstractSubCommand*> _subcommands;
	int _cmdWidthCounter;
	std::string _execname;
public:
	static SubCommandLibrary& getInstance();
	void addSubCommand(AbstractSubCommand& cmd);
	void removeSubCommand(AbstractSubCommand& cmd);
	void getCommandList(std::ostream& output);
	bool getCommandHelp(const std::string& cmd, std::ostream& output);
	int executeCommand(const std::string& cmd, SubParameters& params, std::ostream& output);
	std::string getExecName() { return _execname; }
	void setExecName(const std::string& name) { _execname = name; }
	~SubCommandLibrary();
};