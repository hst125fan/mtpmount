#pragma once
#include <list>
#include <string>
#include <set>

#include <iostream>
#include <Windows.h>

class AbstractTestLineOutputter
{
public:
	virtual void writeLine(std::string content) = 0;
};

class TestLineToCout : public AbstractTestLineOutputter
{
public:
	virtual void writeLine(std::string content) { std::cout << content << std::endl; }
};

class TestLineToOutputWindow : public AbstractTestLineOutputter
{
public:
	virtual void writeLine(std::string content) { OutputDebugStringA( ( content + "\n").c_str()); }
};

class AbstractTest
{
public:
	AbstractTest();
	virtual bool testMain(AbstractTestLineOutputter& dbgprint) = 0;
	virtual std::string getTestName() = 0;
};

class TestDispatch
{
private:
	static TestDispatch* _instance;
	std::string _protfile;
	std::list<AbstractTest*> _tests;
	std::set<std::string> _previouslySucceededTests;
	TestDispatch() { }
public:
	static TestDispatch& getInstance();
	void addTest(AbstractTest* test);
	void loadFile(std::string filename);
	bool executeTests(AbstractTestLineOutputter& dbgprint);
	~TestDispatch();
};