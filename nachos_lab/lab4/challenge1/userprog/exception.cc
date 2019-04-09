// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "noff.h"

static void
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        DEBUG('a', "Shutdown, initiated by user program.\n");
        currentThread->space->clearMap();
        interrupt->Halt();
    }
    
    else if(which == PageFaultException){
        if(machine->TLBPageFaultHandler() == 0)
            return;
        
        currentThread->space->dealWithPageFault();
        
        machine->TLBPageFaultHandler();
    }
    
    else if(which == SyscallException && type == SC_Exit){
        /*printf("\n");
        machine->printTLB();
        printf("\n");*/
        
        currentThread->space->clearMap();
        //machine->printPageBelong();
        printf("%s:A user porgram exit with code %d..\n", currentThread->getName(), machine->ReadRegister(4));
        //int NextPC = machine->ReadRegister(NextPCReg);
        //machine->WriteRegister(PCReg, NextPC);
        //printf("%s finished, tid is %d\n", currentThread->getName(), currentThread->getTID());
        currentThread->Finish();
    }
        
    else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        printf("bad address: %d\n", machine->ReadRegister(BadVAddrReg));
        //for(int i=0; i<NumPhysPages; ++i)
            //printf("%d ", machine->pageBelong[i]);
        printf("%d\n", machine->swapNum);
        unsigned int vpn = (unsigned) machine->ReadRegister(BadVAddrReg) / PageSize;
        ASSERT(FALSE);
    }
}
