// TestAsyncFileCopy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../AsyncFileCopy/CopyFolder.h"
#include "../AsyncFileCopy/AsyncFileCopy.h"

#define STATUS_OK 0

VOID Callback(LPVOID ctx, DWORD status, UINT64 transferedBytes) {
	if (status != STATUS_OK) {
		printf("Error copying files");
		return;
	}

	printf("Successful copy!!!, transfered %lld bytes\n ", transferedBytes);
}

int main()
{
	//talvez criar um ctx de copia com o nome dos ficheiros para um callback mais compreensivo
	AsyncInit();
	CopyFileAsync("ints1.dat", "cpy-ints1.dat", Callback, NULL);
	CopyFileAsync("src.txt", "dst.txt", Callback, NULL);
	AsyncTerminate();

	return 0;
}

