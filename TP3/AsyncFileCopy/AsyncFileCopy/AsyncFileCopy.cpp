// AsyncFileCopy.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "AsyncFileCopy.h"

BOOL AsyncInit()
{
	return FALSE;
}

BOOL CopyFileAsync(PCSTR srcFile, PCSTR dstFile, AsyncCallback cb, LPVOID userCtx)
{
	return FALSE;
}

VOID AsyncTerminate()
{
	
}
