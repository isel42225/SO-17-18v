// AsyncFileCopy.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AsyncFileCopy.h"

BOOL init = FALSE;
BOOL terminated = FALSE;


BOOL AsyncInit()
{
	if (init != FALSE) return FALSE;
	//talvez fazer um primeiro createIOcompletionport e depois associar os pedidos a este  
	return FALSE;
}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx)
{
	return FALSE;
}

VOID AsyncTerminate()
{
	if (terminated != FALSE) return ;
}
