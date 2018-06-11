#pragma once

#ifdef AsyncFileCopyDLL
#define AsyncFileCopy_API __declspec(dllexport)
#else
#define AsyncFileCopy_API  __declspec(dllimport)
#endif

#include <Windows.h>

#define BUFFER_SIZE 4096

typedef VOID(*AsyncCallback) (LPVOID userCtx, DWORD status ,UINT64 transferedBytes);

enum STATE {READ_COMPLETE, WRITE_COMPLETE};	//Current State of operation (Read/Write completed)

typedef struct OperCtx {
	OVERLAPPED ovr;
	HANDLE fIn;
	HANDLE fOut;
	BYTE buffer[BUFFER_SIZE];
	UINT64 currPos;
	AsyncCallback cb;
	STATE state ;
	LPVOID userCtx;
} OPER_CTX, *POPER_CTX;

AsyncFileCopy_API
BOOL AsyncInit();

AsyncFileCopy_API
BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx);

AsyncFileCopy_API
VOID AsyncTerminate();

