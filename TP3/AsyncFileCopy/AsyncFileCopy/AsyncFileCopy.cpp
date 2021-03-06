// AsyncFileCopy.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AsyncFileCopy.h"
#include "stdio.h"
#include <stdlib.h>

#define STATUS_OK 0
#define MAX_THREADS 1 //16

LONG init = NULL;
LONG terminated = NULL;

static HANDLE iocpThreads[MAX_THREADS];
static HANDLE completionPort;


//Completion Port Ops


HANDLE CreateNewCompletionPort(DWORD concurrentThreads)
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, concurrentThreads);
}

BOOL AssociateDeviceWithCompletionPort(HANDLE hComplPort, HANDLE hDevice, DWORD CompletionKey) {
	HANDLE h = CreateIoCompletionPort(hDevice, hComplPort, CompletionKey, 0);
	return h == hComplPort;
}



//OPER_CTX ops

POPER_CTX CreateOpContext(HANDLE fIn, HANDLE fOut, STATE initialState, AsyncCallback cb, LPVOID userCtx) {
	//calloc Allocates an array in memory with elements initialized to 0.
	POPER_CTX op = (POPER_CTX)calloc(1, sizeof(OPER_CTX));
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


//File ops

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

//Wraper for Async Write File func
BOOL WriteAsync(HANDLE hFile, DWORD toWrite, POPER_CTX opCtx) {
	if (!WriteFile(hFile, opCtx->buffer, toWrite, NULL, &opCtx->ovr)) {
		return GetLastError() == ERROR_IO_PENDING;
	}
	return TRUE;
}

//I/O Request Ops

BOOL ProcessRead(POPER_CTX opCtx, DWORD transferedBytes)
{
	// adjust current read position
	opCtx->currPos += transferedBytes;
	LARGE_INTEGER li;
	li.QuadPart = opCtx->currPos;
	// adjust overlapped offset
	opCtx->ovr.Offset = li.LowPart;
	opCtx->ovr.OffsetHigh = li.HighPart;
	if (!ReadAsync(opCtx->fIn, BUFFER_SIZE, opCtx)) 
	//if(!WriteAsync(opCtx->fOut, BUFFER_SIZE, opCtx))
		// error on operation
		return FALSE;

	return TRUE;
}

BOOL ProcessWrite(POPER_CTX opCtx , DWORD toWrite) {
	//LARGE_INTEGER li;
	//li.QuadPart = opCtx->currPos;

	////update Overlapped struct
	//opCtx->ovr.Offset = li.LowPart;
	//opCtx->ovr.OffsetHigh = li.HighPart;
	if (!WriteAsync(opCtx->fOut, toWrite, opCtx))
	//if (!ReadAsync(opCtx->fIn, BUFFER_SIZE, opCtx))
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
		//change state before beggining next oper to avoid thread racing ?
		opCtx->state = READ_COMPLETE;
		if (!ProcessRead(opCtx, transferedBytes))
			goto error;

	}
	else {
		opCtx->state = WRITE_COMPLETE;
		if (!ProcessWrite(opCtx, transferedBytes))
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

		if (&opCtx->ovr == NULL) break;	//Termination request

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
	//using interlocked operation to guarantee thread safety
	if (InterlockedCompareExchange(&init, 1, NULL) == NULL) {
		completionPort = CreateNewCompletionPort(0);	//only 1 thread creates new completion port
		for (int i = 0; i < MAX_THREADS; ++i) 
		{
			iocpThreads[i] = CreateThread(NULL, 0, IOCP_ThreadFunc, NULL, 0, NULL);
		}
		return TRUE;
	}
	return FALSE;
}

BOOL AsyncInit()
{
	return IOCP_Init();
}

VOID AsyncTerminate()
{
	//usar interlock para ver o completion port != null ?
	//não aceitar trabalho futuro
	//esperar que o trabalho pendente acabe
	//talvez usar post completion ... para dizer que a cópia acabou

	if (InterlockedCompareExchange(&terminated, 1, NULL) == NULL)
	{
		//WaitForMultipleObjects(MAX_THREADS, iocpThreads, TRUE, INFINITE);
		
		//Poisoned request, indicates termination
		PostQueuedCompletionStatus(completionPort, NULL, NULL, NULL);
		
		CloseHandle(completionPort);
	}

}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx)
{
	HANDLE src = OpenAsync(srcFile, GENERIC_READ, OPEN_EXISTING);
	HANDLE dst = OpenAsync(dstFile, GENERIC_WRITE, CREATE_ALWAYS);

	POPER_CTX opCtx;
	opCtx = CreateOpContext(src, dst, READ_COMPLETE, cb, userCtx);	//initial state is READ_COMPLETE to do write after first read ?

	if (ReadAsync(src, BUFFER_SIZE, opCtx))
		return TRUE;

	//CloseHandle(src);
	//CloseHandle(dst);			//--> dispatchAndRelease always does this ?
	//DestroyOpContext(opCtx);
	return FALSE;
}