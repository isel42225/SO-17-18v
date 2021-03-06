// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <psapi.h>


#define THRESHOLD 2000
#define STRUCT_SIZE sizeof(PSAPI_WORKING_SET_INFORMATION)
#define WAIT_TIME 5000
#define PAGE_SIZE 4


PSAPI_WORKING_SET_INFORMATION *  getProcessWorkingSet(HANDLE process) {
	PSAPI_WORKING_SET_INFORMATION  wsi;
	ULONG_PTR entries = 1;
	QueryWorkingSet(process, &wsi, STRUCT_SIZE);
	SIZE_T buffer_size = STRUCT_SIZE * wsi.NumberOfEntries;
	PSAPI_WORKING_SET_INFORMATION * res  = (PSAPI_WORKING_SET_INFORMATION *) malloc(buffer_size);
	QueryWorkingSet(process, res , buffer_size);
	return res;
}

void ShowMemInfo(DWORD id) {

	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, false, id);
	PSAPI_WORKING_SET_INFORMATION * p ;

	int countPrev = 0;
	int countNext = 0;

	while (true) {
		p = getProcessWorkingSet(process);
		countPrev = countNext;
		ULONG_PTR process_entries = p->NumberOfEntries;

		for (int i = 0 ; i < process_entries ; ++i) {
			if (p->WorkingSetInfo[i].Shared == 0)
				countNext++;
		}

		int private_pages = countNext - countPrev;

		printf("Process is using %d KB of memory\n", private_pages * PAGE_SIZE);
		printf("Process private pages  = %d\n", private_pages);
		if (countNext >= THRESHOLD) {
			printf("%s\n", "WARNING : Process has memory leak!");
			goto End;
		}
		
		free(p);
		Sleep(WAIT_TIME);
	}

End:
	CloseHandle(process);
	return ;
}


int main(int argc , _TCHAR * argv [])
{

	DWORD id = GetCurrentProcessId();
	if (argc < 2) {
		id = _tstoi(argv[1]);
	}

	ShowMemInfo(id);

}


