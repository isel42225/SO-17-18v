/////////////////////////////////////////////////////////////////
//
// CCISEL 
// 2007-2011
//
// UThread library:
//   User threads supporting cooperative multithreading.
//
// Authors:
//   Carlos Martins, Jo�o Trindade, Duarte Nunes, Jorge Martins
// 

#pragma once

#include <Windows.h>
#include <stdio.h>
#include "UThread.h"
#include "WaitBlock.h"
#include "List.h"
#include <assert.h>
//
// The data structure representing the layout of a thread's execution context
// when saved in the stack.
//



#ifdef _WIN64
//RBX, RBP, RDI, RSI, R12, R13, R14, and R15 
typedef struct _UTHREAD_CONTEXT_64 {
	ULONGLONG R15;
	ULONGLONG R14;
	ULONGLONG R13;
	ULONGLONG R12;
	ULONGLONG RSI;
	ULONGLONG RDI;
	ULONGLONG RBX;
	ULONGLONG RBP;
	VOID (*RetAddr)();
} UTHREAD_CONTEXT_64, *PUTHREAD_CONTEXT_64;

#define PUTHREAD_CONTEXT PUTHREAD_CONTEXT_64
#define UTHREAD_CONTEXT UTHREAD_CONTEXT_64
#else
typedef struct _UTHREAD_CONTEXT_32 {
	ULONG EDI;
	ULONG ESI;
	ULONG EBX;
	ULONG EBP;
	VOID(*RetAddr)();
} UTHREAD_CONTEXT_32, *PUTHREAD_CONTEXT_32;

#define PUTHREAD_CONTEXT PUTHREAD_CONTEXT_32
#define UTHREAD_CONTEXT UTHREAD_CONTEXT_32
#endif

//
//Possible States
//
enum UTSTATE {RUNNING , READY , BLOCKED};

typedef  enum UTSTATE STATE;
	
//.....
typedef struct UtContext {
	DWORD ESP;
	DWORD EIP;
	DWORD EBP;
	DWORD EBX;
	DWORD EDI;
	DWORD ESI;
	// valor entre 0 e 100, que indica
	// a taxa de ocupa��o do stack
	DWORD USED_STACK;
} UT_CONTEXT;



//
// The descriptor of a user thread, containing an intrusive link (through which
// the thread is linked in the ready queue), the thread's starting function and
// argument, the memory block used as the thread's stack and a pointer to the
// saved execution context.
//
typedef struct _UTHREAD {
	PUTHREAD_CONTEXT ThreadContext;
	PCHAR 			 Name;
	BOOL			 Terminate;
	DWORD			 WaitCount;
	STATE			 State;		//Current thread State
	LIST_ENTRY       Link;		//ReadyQueue Link node 
	LIST_ENTRY		 AliveLink;	// the link node for AliveThreads list
	LIST_ENTRY		 Joiners;	// the sentinel node for joiners (waiters for thread termination) list
	UT_FUNCTION      Function;   
	UT_ARGUMENT      Argument; 
	PUCHAR           Stack;
	DWORD			 StackSize;
} UTHREAD, *PUTHREAD;


typedef struct _EXTERN {
	LIST_ENTRY joiners;
	PUTHREAD joined;
}EXTERN, *PEXTERN;


//
// The fixed stack size of a user thread.
//
#define STACK_SIZE (8 * 4096)
