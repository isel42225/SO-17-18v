

#include "stdafx.h"
#include "../Include/UThread.h"
#include "main.h"

void empty(UT_ARGUMENT arg) {}

DWORD start;
DWORD end;
#define NRUNS 1000000 
void ctxSwitch()
{
	
	for (int i = 0; i < NRUNS; ++i)
	{
		start += GetTickCount();
		UtYield();
		end += GetTickCount();
	}
	
}
void initTest()
{
	UtInit();
	UtCreate((PCHAR)"Thread1", 0, empty, NULL);
	UtCreate((PCHAR)"Thread2", 0, empty, NULL);
	UtRun();
}

void endTest()
{
	UtEnd();
}

int main()
{
	initTest();
	ctxSwitch();

	printf("Time for ctxSwitch = %d ms\n", end - start );
	
	endTest();
}



