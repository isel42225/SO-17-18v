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

#include <crtdbg.h>
#include "UThreadInternal.h"

//////////////////////////////////////
//
// UThread internal state variables.
//

//
// The number of existing user threads.
//
static
ULONG NumberOfThreads;

//
// The sentinel of the circular list linking the user threads that are
// currently schedulable. The next thread to run is retrieved from the
// head of this list.
//
static
LIST_ENTRY ReadyQueue;

// The sentinel of the circular list linking the user threads that are
// currently exist.  
//
static LIST_ENTRY AliveThreads;

//
// The currently executing thread.
//
#ifndef _WIN64
static
#endif
PUTHREAD RunningThread;



//
// The user thread proxy of the underlying operating system thread. This
// thread is switched back in when there are no more runnable user threads,
// causing the scheduler to exit.
//
static
PUTHREAD MainThread;

////////////////////////////////////////////////
//
// Forward declaration of internal operations.
VOID __fastcall CleanupThread (PUTHREAD Thread);
//
// The trampoline function that a user thread begins by executing, through
// which the associated function is called.
//
static
VOID InternalStart ();


#ifdef _WIN64
//
// Performs a context switch from CurrentThread to NextThread.
// In x64 calling convention CurrentThread is in RCX and NextThread in RDX.
//
VOID __fastcall  ContextSwitch64 (PUTHREAD CurrentThread, PUTHREAD NextThread);

//
// Frees the resources associated with CurrentThread and switches to NextThread.
// In x64 calling convention  CurrentThread is in RCX and NextThread in RDX.
//
VOID __fastcall InternalExit64 (PUTHREAD Thread, PUTHREAD NextThread);

#define ContextSwitch ContextSwitch64
#define InternalExit InternalExit64

#else

static
VOID __fastcall ContextSwitch32 (PUTHREAD CurrentThread, PUTHREAD NextThread);

//
// Frees the resources associated with CurrentThread and switches to NextThread.
// __fastcall sets the calling convention such that CurrentThread is in ECX
// and NextThread in EDX.
//
static
VOID __fastcall InternalExit32 (PUTHREAD Thread, PUTHREAD NextThread);

#define ContextSwitch ContextSwitch32
#define InternalExit InternalExit32
#endif


////////////////////////////////////////
//
// UThread inline internal operations.
//

//
// Returns and removes the first user thread in the ready queue. If the ready
// queue is empty, the main thread is returned.
//
static
FORCEINLINE
PUTHREAD ExtractNextReadyThread () {
	return IsListEmpty(&ReadyQueue) 
		 ? MainThread 
		 : CONTAINING_RECORD(RemoveHeadList(&ReadyQueue), UTHREAD, Link);
}

//
// Schedule a new thread to run
//
static
FORCEINLINE
VOID Schedule () {
	PUTHREAD NextThread;
    NextThread = ExtractNextReadyThread();
	
	if (NextThread->Terminate == TRUE)
		CleanupThread(NextThread);
	else {
		NextThread->State = RUNNING;
		ContextSwitch(RunningThread, NextThread);
	}

}

///////////////////////////////
//
// UThread public operations.
//

//
// Initialize the scheduler.
// This function must be the first to be called. 
//
VOID UtInit() {
	InitializeListHead(&ReadyQueue);
	InitializeListHead(&AliveThreads);
}

//
// Cleanup all UThread internal resources.
//
VOID UtEnd() {
	/* (this function body was intentionally left empty) */
}

//
// Run the user threads. The operating system thread that calls this function
// performs a context switch to a user thread and resumes execution only when
// all user threads have exited.
//
VOID UtRun () {
	UTHREAD Thread; // Represents the underlying operating system thread.

	//
	// There can be only one scheduler instance running.
	//
	_ASSERTE(RunningThread == NULL);

	//
	// At least one user thread must have been created before calling run.
	//
	if (IsListEmpty(&ReadyQueue)) {
		return;
	}

	//
	// Switch to a user thread.
	//
	MainThread = &Thread;
	RunningThread = MainThread;
	Schedule();
 
	//
	// When we get here, there are no more runnable user threads.
	//
	_ASSERTE(IsListEmpty(&ReadyQueue));
	_ASSERTE(NumberOfThreads == 0);

	//
	// Allow another call to UtRun().
	//
	RunningThread = NULL;
	MainThread = NULL;
}




//
// Terminates the execution of the currently running thread. All associated
// resources are released after the context switch to the next ready thread.
//
VOID UtExit () {
	NumberOfThreads -= 1;	

	// Auto remove from active list
	RemoveEntryList(&RunningThread->AliveLink);

	// awake joined threads
	while (!IsListEmpty(&RunningThread->Joiners)) {
		PLIST_ENTRY tlink = RemoveHeadList(&RunningThread->Joiners);

		PUTHREAD thread = CONTAINING_RECORD(tlink, UTHREAD, Link);

		// put on ready list only when thread is no longer waiting for others threads
		if (thread->WaitCount-- == 0)
			UtActivate(thread);
	}

	InternalExit(RunningThread, ExtractNextReadyThread());
	_ASSERTE(!"Not Supposed to be here!");
}

//
// Relinquishes the processor to the first user thread in the ready queue.
// If there are no ready threads, the function returns immediately.
//
VOID UtYield () {
	if (!IsListEmpty(&ReadyQueue)) {
		InsertTailList(&ReadyQueue, &RunningThread->Link);
		RunningThread->State = READY;	//Added
		Schedule();
	}
}

//
// Returns a HANDLE to the executing user thread.
//
HANDLE UtSelf () {
	return (HANDLE)RunningThread;
}




//
// Halts the execution of the current user thread.
//
VOID UtDeactivate(){
	RunningThread->State = BLOCKED;	//Added
	Schedule();
}


//
// Places the specified user thread at the end of the ready queue, where it
// becomes eligible to run.
//
VOID UtActivate (HANDLE ThreadHandle) {
	PUTHREAD Thread = (PUTHREAD)ThreadHandle;	//Added
	
	if (Thread->Terminate == TRUE)
		CleanupThread(Thread);
	
	else {
		InsertTailList(&ReadyQueue, &(Thread->Link));
		Thread->State = READY;						//Added
	}
}

//
//Return the specified user thread state
//
INT UtThreadState(HANDLE thread) {
	PUTHREAD Thread = (PUTHREAD)thread;
	return Thread->State;
}


// new functions
BOOL UtGetContext(HANDLE ThreadHandle, UT_CONTEXT *ctx) {
	if (UtSelf == ThreadHandle || !UtAlive(ThreadHandle) ) return FALSE;
	PUTHREAD Thread = (PUTHREAD)ThreadHandle;
	PUTHREAD_CONTEXT context = Thread->ThreadContext;
	DWORD StackPointer = (DWORD)context;
	DWORD BaseStack = (DWORD)Thread->Stack;

	DWORD TopStack = BaseStack + STACK_SIZE;
	DWORD RemainingStackSpace = (TopStack - StackPointer  ) * 100 / STACK_SIZE;


	ctx->EBP = context->EBP;
	ctx->EBX = context->EBX;
	ctx->EDI = context->EDI;
	ctx->ESI = context->ESI;
	ctx->EIP = context->RetAddr;
	ctx->ESP = StackPointer;
	ctx->USED_STACK = RemainingStackSpace;
	
	return TRUE;
}

VOID UtTerminateThread(HANDLE tHandle) {
	if (UtSelf() == tHandle) UtExit();

	PUTHREAD t = (PUTHREAD)tHandle;
	t->Terminate = TRUE;

}

BOOL UtMultJoin(HANDLE handle[], int size) {
	PUTHREAD self = (PUTHREAD)UtSelf();
	while (size-- > 0) {
		WAIT_BLOCK wait;
		InitializeWaitBlock(&wait);
		PUTHREAD t = (PUTHREAD)handle[size];
		if (t == self) return FALSE;
		wait.Thread = self;
		t->Joiners = wait.Link;
	}
	UtDeactivate();
	return TRUE;
}

 BOOL UtAlive(HANDLE hThread) {
	PUTHREAD thread = (PUTHREAD)hThread;
	PLIST_ENTRY curr = AliveThreads.Flink;

	while (curr != &AliveThreads) {
		if (curr == &thread->AliveLink)
			return TRUE;
		curr = curr->Flink;
	}
	return FALSE;
}

 //
 //Block running and run nextReadyThread
 //
BOOL UtJoin(HANDLE hthread) {
	if (hthread == UtSelf()) return FALSE;
	if (!UtAlive(hthread)) return TRUE;

	// insert current thread in joiners list da thread hthread
	PUTHREAD thread = (PUTHREAD)hthread;

	InsertTailList(&thread->Joiners, &RunningThread->Link);
	++RunningThread->WaitCount;

	UtDeactivate();  // run other thread and block current thread

	return TRUE;
}

void UtDump()
{
	PLIST_ENTRY  curr = AliveThreads.Flink;	//link to first thread
	PUTHREAD  th;
	for (int i = 0; i < NumberOfThreads; ++i)
	{
		th = CONTAINING_RECORD(curr, UTHREAD, AliveLink);
		PUTHREAD_CONTEXT context = th->ThreadContext;
		DWORD StackPointer = (DWORD)context;
		DWORD BaseStack = (DWORD)th->Stack;
		HANDLE hT = (HANDLE)th;
		DWORD TopStack = BaseStack + th->StackSize ;
		DWORD UsedStack = (TopStack - StackPointer) * 100 / STACK_SIZE;

		printf("Handle = %p,  Name = %s , Ocupied Stack = %d", hT, th->Name, UsedStack);
		curr = curr->Flink;
	}
}


///////////////////////////////////////
//
// Definition of internal operations.
//

//
// The trampoline function that a user thread begins by executing, through
// which the associated function is called.
//
VOID InternalStart () {
	RunningThread->Function(RunningThread->Argument);
	UtExit(); 
}


//
// Frees the resources associated with Thread..
//
VOID __fastcall CleanupThread (PUTHREAD Thread) {
	free(Thread->Stack);
	free(Thread);
}

//
// functions with implementation dependent of X86 or x64 platform
//

#ifndef _WIN64

//
// Creates a user thread to run the specified function. The thread is placed
// at the end of the ready queue.
// Added name and custom stack size
HANDLE UtCreate32 (PCHAR Name , DWORD StackSize, UT_FUNCTION Function, UT_ARGUMENT Argument) {
	PUTHREAD Thread;
	
	//
	// Dynamically allocate an instance of UTHREAD and the associated stack.
	//
	Thread = (PUTHREAD) malloc(sizeof (UTHREAD));
	Thread->Name = Name;
	if (StackSize == 0) StackSize = STACK_SIZE;
	//Thread->Stack = (PUCHAR) malloc(STACK_SIZE);
	Thread->Stack = (PUCHAR)malloc(StackSize);
	Thread->StackSize = StackSize;
	_ASSERTE(Thread != NULL && Thread->Stack != NULL);

	//
	// Zero the stack for emotional confort.
	//
	memset(Thread->Stack, 0, StackSize);

	//
	// Memorize Function and Argument for use in InternalStart.
	//
	Thread->Function = Function;
	Thread->Argument = Argument;

	//
	// Map an UTHREAD_CONTEXT instance on the thread's stack.
	// We'll use it to save the initial context of the thread.
	//
	// +------------+
	// | 0x00000000 |    <- Highest word of a thread's stack space
	// +============+       (needs to be set to 0 for Visual Studio to
	// |  RetAddr   | \     correctly present a thread's call stack).
	// +------------+  |
	// |    EBP     |  |
	// +------------+  |
	// |    EBX     |   >   Thread->ThreadContext mapped on the stack.
	// +------------+  |
	// |    ESI     |  |
	// +------------+  |
	// |    EDI     | /  <- The stack pointer will be set to this address
	// +============+       at the next context switch to this thread.
	// |            | \
	// +------------+  |
	// |     :      |  |
	//       :          >   Remaining stack space.
	// |     :      |  |
	// +------------+  |
	// |            | /  <- Lowest word of a thread's stack space
	// +------------+       (Thread->Stack always points to this location).
	//

	Thread->ThreadContext = (PUTHREAD_CONTEXT) (Thread->Stack +
		STACK_SIZE - sizeof (ULONG) - sizeof (UTHREAD_CONTEXT));

	//
	// Set the thread's initial context by initializing the values of EDI,
	// EBX, ESI and EBP (must be zero for Visual Studio to correctly present
	// a thread's call stack) and by hooking the return address.
	// 
	// Upon the first context switch to this thread, after popping the dummy
	// values of the "saved" registers, a ret instruction will place the
	// address of InternalStart on EIP.
	//
	Thread->ThreadContext->EDI = 0x33333333;
	Thread->ThreadContext->EBX = 0x11111111;
	Thread->ThreadContext->ESI = 0x22222222;
	Thread->ThreadContext->EBP = 0x00000000;
	Thread->ThreadContext->RetAddr = InternalStart;


	// Start of new Stuff
		// Add to active list
		
		InsertTailList(&AliveThreads, &Thread->AliveLink);


	// Initialize Joiners list
	InitializeListHead(&Thread->Joiners);
	// End of new Stuff 

	//
	// Ready the thread.
	//
	NumberOfThreads += 1;
	UtActivate((HANDLE)Thread);
	
	
	return (HANDLE)Thread;
}

//
// Performs a context switch from CurrentThread to NextThread.
// __fastcall sets the calling convention such that CurrentThread is in ECX and NextThread in EDX.
// __declspec(naked) directs the compiler to omit any prologue or epilogue.
//
__declspec(naked) 
VOID __fastcall ContextSwitch32 (PUTHREAD CurrentThread, PUTHREAD NextThread) {
	__asm {
		// Switch out the running CurrentThread, saving the execution context on the thread's own stack.   
		// The return address is atop the stack, having been placed there by the call to this function.
		//
		push	ebp
		push	ebx
		push	esi
		push	edi
		//
		// Save ESP in CurrentThread->ThreadContext.
		//
		mov		dword ptr [ecx].ThreadContext, esp
		//
		// Set NextThread as the running thread.
		//
		mov     RunningThread, edx
		//
		// Load NextThread's context, starting by switching to its stack, where the registers are saved.
		//
		mov		esp, dword ptr [edx].ThreadContext

		pop		edi
		pop		esi
		pop		ebx
		pop		ebp
		//
		// Jump to the return address saved on NextThread's stack when the function was called.
		//
		ret
	}
}

//
// Frees the resources associated with CurrentThread and switches to NextThread.
// __fastcall sets the calling convention such that CurrentThread is in ECX and NextThread in EDX.
// __declspec(naked) directs the compiler to omit any prologue or epilogue.
//
__declspec(naked)
VOID __fastcall InternalExit32 (PUTHREAD CurrentThread, PUTHREAD NextThread) {
	__asm {

		//
		// Set NextThread as the running thread.
		//
		mov     RunningThread, edx
		
		//
		// Load NextThread's stack pointer before calling CleanupThread(): making the call while
		// using CurrentThread's stack would mean using the same memory being freed -- the stack.
		//
		mov		esp, dword ptr [edx].ThreadContext

		call    CleanupThread

		//
		// Finish switching in NextThread.
		//
		pop		edi
		pop		esi
		pop		ebx
		pop		ebp
		ret
	}
}

#else

//
// Creates a user thread to run the specified function. The thread is placed
// at the end of the ready queue.
//
HANDLE UtCreate64 (UT_FUNCTION Function, UT_ARGUMENT Argument) {
	PUTHREAD Thread;
	
	//
	// Dynamically allocate an instance of UTHREAD and the associated stack.
	//
	Thread = (PUTHREAD) malloc(sizeof (UTHREAD));
	Thread->Stack = (PUCHAR) malloc(STACK_SIZE);
	_ASSERTE(Thread != NULL && Thread->Stack != NULL);

	//
	// Zero the stack for emotional confort.
	//
	memset(Thread->Stack, 0, STACK_SIZE);

	//
	// Memorize Function and Argument for use in InternalStart.
	//
	Thread->Function = Function;
	Thread->Argument = Argument;

	//
	// Map an UTHREAD_CONTEXT instance on the thread's stack.
	// We'll use it to save the initial context of the thread.
	//
	// +------------+  <- Highest word of a thread's stack space
	// | 0x00000000 |    (needs to be set to 0 for Visual Studio to
	// +------------+      correctly present a thread's call stack).   
	// | 0x00000000 |  \
	// +------------+   |
	// | 0x00000000 |   | <-- Shadow Area for Internal Start 
	// +------------+   |
	// | 0x00000000 |   |
	// +------------+   |
	// | 0x00000000 |  /
	// +============+       
	// |  RetAddr   | \    
	// +------------+  |
	// |    RBP     |  |
	// +------------+  |
	// |    RBX     |   >   Thread->ThreadContext mapped on the stack.
	// +------------+  |
	// |    RDI     |  |
	// +------------+  |
	// |    RSI     |  |
	// +------------+  |
	// |    R12     |  |
	// +------------+  |
	// |    R13     |  |
	// +------------+  |
	// |    R14     |  |
	// +------------+  |
	// |    R15     | /  <- The stack pointer will be set to this address
	// +============+       at the next context switch to this thread.
	// |            | \
	// +------------+  |
	// |     :      |  |
	//       :          >   Remaining stack space.
	// |     :      |  |
	// +------------+  |
	// |            | /  <- Lowest word of a thread's stack space
	// +------------+       (Thread->Stack always points to this location).
	//

	Thread->ThreadContext = (PUTHREAD_CONTEXT) (Thread->Stack +
		STACK_SIZE -sizeof (UTHREAD_CONTEXT)-sizeof(ULONGLONG)*5);

	//
	// Set the thread's initial context by initializing the values of 
	// registers that must be saved by the called (R15,R14,R13,R12, RSI, RDI, RBCX, RBP)
	
	// 
	// Upon the first context switch to this thread, after poppi
	ng the dummy
	// values of the "saved" registers, a ret instruction will place the
	// address of InternalStart on EIP.
	//
	Thread->ThreadContext->R15 = 0x77777777;
	Thread->ThreadContext->R14 = 0x66666666;
	Thread->ThreadContext->R13 = 0x55555555;
	Thread->ThreadContext->R12 = 0x44444444;	
	Thread->ThreadContext->RSI = 0x33333333;
	Thread->ThreadContext->RDI = 0x22222222;
	Thread->ThreadContext->RBX = 0x11111111;
	Thread->ThreadContext->RBP = 0x00000000;		
	Thread->ThreadContext->RetAddr = InternalStart;

	// Start of new Stuff 
	// Add to active list
	InsertTailList(&AliveThreads, &Thread->AliveLink);


	// Initialize Joiners list
	InitializeListHead(&Thread->Joiners);
	// End of new Stuff 

	//
	// Ready the thread.
	//
	NumberOfThreads += 1;
	UtActivate((HANDLE)Thread);
	
	return (HANDLE)Thread;
}




#endif