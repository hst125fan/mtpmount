

#pragma once
#include "..\mtpaccess\Utils.h"
#include <thread>

struct testCritResource
{
	std::string* actualResource;
	ParalellReadUniqueWriteSync* syncer;
	std::string inOrBackWrite;
	int sleeptime;
	std::atomic<int>* paralellWrites;
	std::atomic<int>* paralellReads;
};

void troutine1(testCritResource* res)
{
	res->syncer->getReadAccess();
	(*(res->paralellReads))++;
	res->inOrBackWrite = *(res->actualResource);
	if (res->sleeptime > 0) { Sleep(res->sleeptime); }
	(*(res->paralellReads))--;
	res->syncer->endReadAccess();
}

void troutine2(testCritResource* res)
{
	res->syncer->getWriteAccess();
	(*(res->paralellWrites))++;
	*(res->actualResource) = res->inOrBackWrite;
	if (res->sleeptime > 0) { Sleep(res->sleeptime); }
	(*(res->paralellWrites))--;
	res->syncer->endWriteAccess();
}