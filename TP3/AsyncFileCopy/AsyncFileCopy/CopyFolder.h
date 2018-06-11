#pragma once

#include <Windows.h>
#include "AsyncFileCopy.h"

#ifdef CopyFolderDLL
#define CopyFolder_API __declspec(dllexport)
#else
#define CopyFolder_API  __declspec(dllimport)
#endif

CopyFolder_API
DWORD CopyFolder(PCSTR origFolder, PCSTR dstFolder);
