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

void CreateFunc(){
    printf("System Call Create..\n");
    //获取参数name字符串的地址
    int nameaddr = machine->ReadRegister(4);
    int value = 1;
    int namelen = 0;
    //获取文件名字长度，这里认为文件名长度没有固定大小限制
    for (; value != 0;) {
        machine->ReadMem(nameaddr++, 1, &value);
        if (value != 0)
            namelen++;
        else
            break;
    }
    //获取文件名
    char *filename = new char[namelen + 1];
    int i;
    nameaddr = machine->ReadRegister(4);
    for (i = 0; i < namelen; i++) {
        machine->ReadMem(nameaddr + i, 1, &value);
        filename[i] = (char) value;
    }
    filename[i] = '\0';
    printf("Create the File: %s\n", filename);
    fileSystem->Create(filename, 128);
    machine->AddvancePC();
}

void OpenFunc(){
    printf("System Call Open..\n");
    //获取参数name字符串的地址
    int nameaddr = machine->ReadRegister(4);
    int value = 1;
    int namelen = 0;
    //获取文件名字长度，这里认为文件名长度没有固定大小限制
    for (; value != 0;) {
        machine->ReadMem(nameaddr++, 1, &value);
        if (value != 0)
            namelen++;
        else
            break;
    }
    //获取文件名
    char *filename = new char[namelen + 1];
    int i;
    nameaddr = machine->ReadRegister(4);
    for (i = 0; i < namelen; i++) {
        machine->ReadMem(nameaddr + i, 1, &value);
        filename[i] = (char) value;
    }
    filename[i] = '\0';
    printf("Open the file: %s\n", filename);
    //将OpenFile数据结构转化为整数
    OpenFile *tmpopenfile = fileSystem->Open(filename);
    machine->WriteRegister(2, int(tmpopenfile));
    printf("The Openfile ID is %d\n", int(tmpopenfile));
    machine->AddvancePC();
}

void CloseFunc(){
    printf("System Call Close..\n");
    //获取OpenFileId
    int id = machine->ReadRegister(4);
    printf("Close Openfile %d\n", id);
    OpenFile *openfile = (OpenFile *) id;
    delete openfile;
    machine->AddvancePC();
}

void WriteFunc(){
    printf("System Call Write..\n");
    //获取buffer的地址,size,id
    int bufferaddr = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);
    int id = machine->ReadRegister(6);
    //获取所有需要写的内容
    char *buffer = new char[size + 1];
    for (int i = 0; i < size; i++) {
        int value;
        machine->ReadMem(bufferaddr + i, 1, &value);
        buffer[i] = (char) value;
    }
    OpenFile *openfile = (OpenFile *) id;
    openfile->Write(buffer, size);
    machine->AddvancePC();
}

void ReadFunc(){
    printf("System Call Read..\n");
    //获取buffer的地址,size,id
    int bufferaddr = machine->ReadRegister(4);
    int size = machine->ReadRegister(5);
    int id = machine->ReadRegister(6);
    //从bufferaddr开始写size个字符
    OpenFile *op = (OpenFile *) id;
    char *buffer = new char[size + 1];
    int res = op->Read(buffer, size);
    for (int i = 0; i < res; i++) {
        machine->WriteMem(bufferaddr + i, 1, int(buffer[i]));
    }
    buffer[size] = '\0';
    printf("Read from Openfile %d: %s\n", id, buffer);
    machine->WriteRegister(2, res);
    machine->AddvancePC();
}

void execfunc(int arg) {
    //根据传进的参数获取可执行文件名
    char *filename = (char *) arg;
    printf("Execute File %s\n", filename);
    //仿照StartProcess中步骤
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *addr = new AddrSpace(executable);
    addr->filename = filename;
    currentThread->space = addr;
    currentThread->setName(filename);
    delete executable;
    //printf("initial the registers\n");
    addr->InitRegisters();
    addr->RestoreState();
    currentThread->SaveUserState();
    //printf("start to run the executable\n");
    machine->Run();
}

void ExecFunc(){
    printf("System Call Exec..\n");
    //获取参数name字符串的地址
    int nameaddr = machine->ReadRegister(4);
    int value = 1;
    int namelen = 0;
    //获取文件名字长度，这里认为文件名长度没有固定大小限制
    for (; value != 0;) {
        machine->ReadMem(nameaddr++, 1, &value);
        if (value != 0)
            namelen++;
        else
            break;
    }
    //获取可执行文件名
    char *filename = new char[namelen + 1];
    int i;
    nameaddr = machine->ReadRegister(4);
    for (i = 0; i < namelen; i++) {
        machine->ReadMem(nameaddr + i, 1, &value);
        filename[i] = (char) value;
    }
    filename[i] = '\0';
    //创建一个新线程来执行指定函数
    Thread *newthread = new Thread("Thread");
    //        printf(".......%d %d \n", TIDstate[newthread->getTID()][0], TIDstate[newthread->getTID()][1]);
    printf("CurrentThread is %d,\n", currentThread->getTID());
    printf("the Thread to execute the executable is thread %d\n", newthread->getTID());
    machine->WriteRegister(2, newthread->getTID());
    newthread->Fork(execfunc, (int) filename);
    machine->AddvancePC();
}


void forkfunc(int pc){
    //此时已经进入Fork的线程，所以所有操作针对的应该是当前线程
    //地址空间已在处理函数中复制
    //初始化寄存器到machine
    currentThread->space->InitRegisters();
    //装载页表到machine
    currentThread->space->RestoreState();
    //根据传入参数设置PC
    machine->WriteRegister(PCReg, pc);
    machine->WriteRegister(NextPCReg, pc + 4);
    machine->Run();
}

void ForkFunc(){
    printf("System Call Fork..\n");
    //从r4中获取需要新线程执行的函数地址
    int func = machine->ReadRegister(4);
    //复制当前的地址空间到新线程
    Thread *newthread = new Thread("forked thread");
    newthread->space = currentThread->space;
    //将func地址传递给新线程
    newthread->Fork(forkfunc, func);
    //printf("nonono\n");
    machine->AddvancePC();
}

void YieldFunc(){
    printf("System Call Yield..\n");
    currentThread->Yield();
    //printf("I can return!\n");
    machine->AddvancePC();
}

void JoinFunc(){
    printf("System Call Join..\n");
    currentThread->SaveUserState();
    
    int spaceid = machine->ReadRegister(4);
    //spaceid表示要Join的线程的ID
    printf("Thread %d Waiting for Thread %d to Finish..\n", currentThread->getTID(), spaceid);
    while (myThreads[spaceid] != NULL) { //等待相应的线程结束
        currentThread->Yield();
    }
    printf("Thread %d exit with code %d\n", spaceid, ThreadExit[spaceid]);
    //写返回值
    machine->WriteRegister(2, ThreadExit[spaceid]);
    /*printf("current PC is %d, next pc is %d, status is %d\n", machine->ReadRegister(PCReg),
           machine->ReadRegister(NextPCReg), currentThread->getstatus());
    printf("return address is %d\n", machine->ReadRegister(RetAddrReg));*/
    //currentThread->PrintState();
    machine->AddvancePC();
}

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
        ThreadExit[currentThread->getTID()] = machine->ReadRegister(4);
        
        machine->AddvancePC();
        
        currentThread->Finish();
    }
    
    else if(which == SyscallException && type == SC_Create){
        CreateFunc();
    }
    
    else if(which == SyscallException && type == SC_Open){
        OpenFunc();
    }
    
    else if(which == SyscallException && type == SC_Close){
        CloseFunc();
    }
    
    else if(which == SyscallException && type == SC_Write){
        WriteFunc();
    }
    
    else if(which == SyscallException && type == SC_Read){
        ReadFunc();
    }
    
    else if(which == SyscallException && type == SC_Exec){
        ExecFunc();
    }
    
    else if(which == SyscallException && type == SC_Fork){
        ForkFunc();
    }
    
    else if(which == SyscallException && type == SC_Yield){
        YieldFunc();
    }
    
    else if(which == SyscallException && type == SC_Join){
        JoinFunc();
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
