#include "stdafx.h"
#include <Windows.h>


#define NSWITCH 100000

DWORD start;
DWORD end;

void SameProcessCommute()
{
	
	for (int i = 0; i < NSWITCH; ++i)
	{
		start +=  GetTickCount();
		SwitchToThread();
		end += GetTickCount();
	}

	printf("Time elapsed = %d\n", (end - start) / 2); 
	
	start = 0;
	end = 0;
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

