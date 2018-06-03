// AsyncFileCopy.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AsyncFileCopy.h"
#include "stdio.h"

#define STATUS_OK 0
#define MAX_THREADS 16

BOOL init = FALSE;
BOOL terminated = FALSE;

static HANDLE iocpThreads[MAX_THREADS];
static HANDLE completionPort;

HANDLE CreateNewCompletionPort(DWORD concurrentThreads)
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, concurrentThreads);
}

BOOL AssociateDeviceWithCompletionPort(HANDLE hComplPort, HANDLE hDevice, DWORD CompletionKey) {
	HANDLE h = CreateIoCompletionPort(hDevice, hComplPort, CompletionKey, 0);
	return h == hComplPort;
}

POPER_CTX CreateOpContext(HANDLE fIn, HANDLE fOut, AsyncCallback cb, LPVOID userCtx) {
	POPER_CTX op = (POPER_CTX)calloc(1, sizeof(OPER_CTX));
	op->cb = cb;
	op->userCtx = userCtx;
	op->fIn = fIn;
	op->fOut = fOut;

	return op;
}

VOID DestroyOpContext(POPER_CTX ctx) {
	CloseHandle(ctx->fIn);
	free(ctx);
}

VOID DispatchAndReleaseOper(POPER_CTX opCtx, DWORD status) {
	opCtx->cb(opCtx->userCtx, status, opCtx->currPos);	//curr pos is equal to transfered bytes
	DestroyOpContext(opCtx);
}

DWORD WINAPI IOCP_ThreadFunc(LPVOID arg) {
	DWORD transferedBytes;
	ULONG_PTR completionKey;
	POPER_CTX opCtx;

	while (TRUE) {
		BOOL res = GetQueuedCompletionStatus(completionPort,
			&transferedBytes, &completionKey, (LPOVERLAPPED *)&opCtx, INFINITE);

		if (!res) {
			transferedBytes = 0;
			DWORD error = GetLastError();
			if (error != ERROR_HANDLE_EOF) {
				// operation error, abort calling callback
				DispatchAndReleaseOper(opCtx, error);
				continue;
			}
		}
		ProcessRequest(opCtx, transferedBytes);
	}
	return 0;
}

VOID ProcessRequest(POPER_CTX opCtx, DWORD transferedBytes) {
	if (transferedBytes == 0) { // operation done, call callback!
		DispatchAndReleaseOper(opCtx, STATUS_OK);
		return;
	}
	
	if (opCtx->read)
	{
		if (!ProcessRead(opCtx, transferedBytes))
			goto error;
	}
	else {
		if (!ProcessWrite(opCtx, transferedBytes))
			goto error;
	}
	return;
error:
	DispatchAndReleaseOper(opCtx, GetLastError());
}

BOOL ProcessRead(POPER_CTX opCtx, DWORD transferedBytes) 
{
	// adjust current read position
	LARGE_INTEGER li;
	opCtx->currPos += transferedBytes;
	li.QuadPart = opCtx->currPos;
	// adjust overlapped offset
	opCtx->ovr.Offset = li.LowPart;
	opCtx->ovr.OffsetHigh = li.HighPart;
	if (!ReadAsync(opCtx->fIn, BUFFER_SIZE, opCtx)) {
		// error on operation
		return FALSE;
		
	}
	return TRUE;
}

BOOL ProcessWrite(POPER_CTX opCtx, DWORD transferedBytes) {

}

BOOL IOCP_Init() {
	completionPort = CreateNewCompletionPort(0);
	if (completionPort == NULL) return FALSE;
	for (int i = 0; i < MAX_THREADS; ++i) {
		iocpThreads[i] = CreateThread(NULL, 0, IOCP_ThreadFunc, NULL, 0, NULL);

	}
	return TRUE;
}

//Used to open existing or to create new file
HANDLE OpenAsync(PCSTR fName, DWORD permissions , DWORD creation) {
	HANDLE hFile = CreateFileA(fName, permissions,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		creation,
		FILE_FLAG_OVERLAPPED,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) return NULL;
	if (!AssociateDeviceWithCompletionPort(completionPort, hFile, (ULONG_PTR)hFile)) {
		CloseHandle(hFile);
		return NULL;
	}
	return hFile;
}

//Wraper for Async Read File func
BOOL ReadAsync(HANDLE hFile, DWORD toRead, POPER_CTX opCtx) {
	if (!ReadFile(hFile, opCtx->buffer, toRead, NULL, &opCtx->ovr)) {
		return GetLastError() == ERROR_IO_PENDING;
	}
	return TRUE;
}

//Wraper for Async Write File func
BOOL WriteAsync(HANDLE hFile, DWORD toWrite, POPER_CTX opCtx) {
	if (!WriteFile(hFile, opCtx->buffer, toWrite, NULL, &opCtx->ovr)) {
		return GetLastError() == ERROR_IO_PENDING;
	}
	return TRUE;
}

BOOL AsyncInit()
{
	if (init != FALSE) return FALSE;
	
	IOCP_Init();
	return FALSE;
}


VOID Callback(LPVOID ctx, DWORD status) {
	if (status != STATUS_OK) {
		printf("Error copying files");
		return;
	}

	printf("Successful copy!!!");
}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx)
{
	HANDLE src = OpenAsync(srcFile, GENERIC_READ, OPEN_EXISTING);
	HANDLE dst = OpenAsync(dstFile, GENERIC_WRITE, CREATE_ALWAYS);
	return FALSE;
}

VOID AsyncTerminate()
{
	if (terminated != FALSE) return ;
}


