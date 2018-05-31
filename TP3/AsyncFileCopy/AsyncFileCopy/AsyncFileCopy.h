#pragma once 
#include <Windows.h>


typedef VOID(*AsyncCallback) (LPVOID userCtx, DWORD status ,UINT64 transferedBytes);


BOOL AsyncInit();

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx);

VOID AsyncTerminate();