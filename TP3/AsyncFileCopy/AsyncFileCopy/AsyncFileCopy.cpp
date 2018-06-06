// AsyncFileCopy.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AsyncFileCopy.h"
#include "stdio.h"
#include <stdlib.h>

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

POPER_CTX CreateOpContext(HANDLE fIn, HANDLE fOut, STATE initialState,  AsyncCallback cb, LPVOID userCtx) {
	POPER_CTX op = (POPER_CTX)calloc(1, sizeof(OPER_CTX));	//calloc Allocates an array in memory with elements initialized to 0.
	op->cb = cb;
	op->userCtx = userCtx;
	op->fIn = fIn;
	op->fOut = fOut;
	op->state = initialState;

	return op;
}

VOID DestroyOpContext(POPER_CTX ctx) {
	CloseHandle(ctx->fIn);
	CloseHandle(ctx->fOut);
	free(ctx);
}

VOID DispatchAndReleaseOper(POPER_CTX opCtx, DWORD status) {
	opCtx->cb(opCtx->userCtx, status, opCtx->currPos);	//curr pos is equal to transfered bytes
	DestroyOpContext(opCtx);
}

//Used to open existing or to create new file
HANDLE OpenAsync(PCSTR fName, DWORD permissions, DWORD creation) {
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

BOOL ProcessRead(POPER_CTX opCtx, DWORD transferedBytes)
{
	// adjust current read position
	opCtx->currPos += transferedBytes;
	LARGE_INTEGER li;
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


//Wraper for Async Write File func
BOOL WriteAsync(HANDLE hFile, DWORD toWrite, POPER_CTX opCtx) {
	if (!WriteFile(hFile, opCtx->buffer, toWrite, NULL, &opCtx->ovr)) {
		return GetLastError() == ERROR_IO_PENDING;
	}
	return TRUE;
}

BOOL ProcessWrite(POPER_CTX opCtx) {
	LARGE_INTEGER li;
	li.QuadPart = opCtx->currPos;

	//update Overlapped struct
	opCtx->ovr.Offset = li.LowPart;
	opCtx->ovr.OffsetHigh = li.HighPart;
	if (!WriteAsync(opCtx->fOut, BUFFER_SIZE, opCtx))
		return FALSE;
	return TRUE;
}

//General Request processor , checks if next oper is read or write
VOID ProcessRequest(POPER_CTX opCtx, DWORD transferedBytes) {
	if (transferedBytes == 0) { // operation done, call callback!
		DispatchAndReleaseOper(opCtx, STATUS_OK);
		return;
	}

	if (opCtx->state == WRITE_COMPLETE)
	{
		opCtx->state = READ_COMPLETE;	//change state before beggining next oper to avoid thread racing ?
		if (!ProcessRead(opCtx, transferedBytes))
			goto error;
		
	}
	else {
		opCtx->state = WRITE_COMPLETE;
		if (!ProcessWrite(opCtx))
			goto error;
	}
	return;

error:
	DispatchAndReleaseOper(opCtx, GetLastError());
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


BOOL IOCP_Init() {
	completionPort = CreateNewCompletionPort(0);
	if (completionPort == NULL) return FALSE;
	for (int i = 0; i < MAX_THREADS; ++i) {
		iocpThreads[i] = CreateThread(NULL, 0, IOCP_ThreadFunc, NULL, 0, NULL);

	}
	return TRUE;
}

BOOL AsyncInit()
{
	if (init != FALSE) return FALSE;	//flag inutil , usar interlock para ver o completion port != null ?
	
	IOCP_Init();
	return FALSE;
}


VOID Callback(LPVOID ctx, DWORD status, UINT64 transferedBytes) {
	if (status != STATUS_OK) {
		printf("Error copying files");
		return;
	}

	printf("Successful copy!!!, transfered %lld bytes", transferedBytes);
}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx)
{
	HANDLE src = OpenAsync(srcFile, GENERIC_READ, OPEN_EXISTING);
	HANDLE dst = OpenAsync(dstFile, GENERIC_WRITE, CREATE_ALWAYS);

	POPER_CTX opCtx;
	opCtx = CreateOpContext(src, dst, WRITE_COMPLETE, Callback, userCtx);	//initial state is WRITE_COMPLETE to do first read ?

	if (ReadAsync(src, BUFFER_SIZE, opCtx) && WriteAsync(dst, BUFFER_SIZE, opCtx))
		return TRUE;
		
	/*CloseHandle(src);
	CloseHandle(dst);			--> dispatchAndRelease always does this ?
	DestroyOpContext(opCtx);*/
	return FALSE;
}

VOID AsyncTerminate()
{
	if (terminated != FALSE) return ;	//flag inutil , usar interlock para ver o completion port != null ?
	//não aceitar trabalho futuro
	//esperar que o trabalho idependente acaba 
	//talvez usar post completion ... para dizer que a cópia acabou
	CloseHandle(completionPort);
}


