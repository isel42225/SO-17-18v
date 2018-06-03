#pragma once 
#include <Windows.h>

#define BUFFER_SIZE 4096

typedef VOID(*AsyncCallback) (LPVOID userCtx, DWORD status ,UINT64 transferedBytes);

typedef struct OperCtx {
	OVERLAPPED ovr;
	HANDLE fIn;
	HANDLE fOut;
	BYTE buffer[BUFFER_SIZE];
	UINT64 currPos;
	AsyncCallback cb;
	DWORD r_w;	//signal if is read or write
	LPVOID userCtx;
} OPER_CTX, *POPER_CTX;


BOOL AsyncInit();

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx);

VOID AsyncTerminate();

