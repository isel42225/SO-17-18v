#include "stdafx.h"
#include <Windows.h>


#define NSWITCH 10000


void SameProcessCommute()
{
	DWORD start = GetTickCount();
	for (int i = 0; i < NSWITCH; ++i)
	{
		SwitchToThread();
	}

	DWORD end = GetTickCount();

	printf("Time elapsed = %d\n", (end - start) / 2); // ???

}
 
void OtherProcessCommute()
{

	STARTUPINFO si = {sizeof(STARTUPINFO)};
	PROCESS_INFORMATION pi;
	BOOL b = CreateProcess(NULL, NULL, NULL, NULL, TRUE, 0, NULL, NULL, &si , &pi);
}

int main()
{

	SameProcessCommute();

}

