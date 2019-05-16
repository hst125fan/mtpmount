
#include "TestDispatch.h"
#include "../mtpaccess/Utils.h"

#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>


AbstractTest::AbstractTest()
{
	TestDispatch::getInstance().addTest(this);
}

TestDispatch& TestDispatch::getInstance()
{
	if (_instance == NULL)
	{
		_instance = new TestDispatch;
	}
	return *_instance;
}

void TestDispatch::addTest(AbstractTest* test)
{
	_tests.push_back(test);
}

void TestDispatch::loadFile(std::string filename)
{
	_protfile = filename;
	std::ifstream prot(filename);
	while (prot.good())
	{
		std::string protline;
		std::getline(prot, protline);
		if (protline.empty()) { continue; }
		_previouslySucceededTests.insert(protline);
	}
}

bool TestDispatch::executeTests(AbstractTestLineOutputter& dbgprint)
{
	AutoDeletePointer<CacheLocationProvider> deleteCacheLocProvOnExit(&(CacheLocationProvider::getInstance()));
	int successful = 0;
	int failed = 0;
	int total = static_cast<int>( _tests.size() );
	int whereAmI = 0;
	std::ofstream prot(_protfile, std::ios::trunc);
	dbgprint.writeLine("Performing " + std::to_string(total) + " tests...");
	for (std::list<AbstractTest*>::iterator iter = _tests.begin();
		iter != _tests.end();
		iter++)
	{
		whereAmI++;
		std::string nameOfCurrentTest((*iter)->getTestName());
		
		if (_previouslySucceededTests.end() != _previouslySucceededTests.find(nameOfCurrentTest))
		{
			dbgprint.writeLine("Skipping Test " + nameOfCurrentTest + ", as it already ran successful");
			successful++;
			prot << nameOfCurrentTest << std::endl;
			continue;
		}

		dbgprint.writeLine("Test " + std::to_string(whereAmI) + " of " + std::to_string(total) + ": " + nameOfCurrentTest);
		bool succeeded = false;
		try
		{
			succeeded = (*iter)->testMain(dbgprint);
		}
		catch (std::exception &e)
		{
			dbgprint.writeLine("Exception caught: " + std::string(e.what()));
		}
		catch (...)
		{
			dbgprint.writeLine("An unknown exception occured while executing test " + std::to_string(whereAmI));
			succeeded = false;
		}
		if (succeeded)
		{
			successful++;
			prot << nameOfCurrentTest << std::endl;
		}
		else
		{
			failed++;
			dbgprint.writeLine((*iter)->getTestName() + " failed!");
		}
	}
	dbgprint.writeLine("Test finished, " + std::to_string(successful) + " succeeded and " + std::to_string(failed) + " failed");
	return (failed == 0) ? true : false;
}

TestDispatch::~TestDispatch()
{
	for (std::list<AbstractTest*>::iterator iter = _tests.begin();
		iter != _tests.end();
		iter++)
	{
		delete (*iter);
	}
}

TestDispatch* TestDispatch::_instance = NULL;

#define defNewTest( testname ) class testname : public AbstractTest { virtual std::string getTestName() { return std::string(#testname); }  virtual bool testMain(AbstractTestLineOutputter& dbgprint); }; bool testname##::testMain(AbstractTestLineOutputter& dbgprint)
#define defTestActivateHeaderInclude
#define output( string ) dbgprint.writeLine( string )
#include "all_test_headers.h"
#undef defNewTest
#undef output
#undef defTestActivateHeaderInclude




int main(int argc, char** argv)
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);

	TestLineToCout dbgprint;

#define defNewTest( testname ) testname##* testname##_instance = new testname; if(0)
#define output( string )
#include "all_test_headers.h"
#undef defNewTest
#undef output

	TestDispatch::getInstance().loadFile(argv[1]);

	AutoDeletePointer<TestDispatch> autoDelete( &(TestDispatch::getInstance()) );
	
	return (TestDispatch::getInstance().executeTests(dbgprint)) ? 0 : 1;
}
