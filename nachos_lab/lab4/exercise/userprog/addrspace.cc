// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

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
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;
    
    progMap = new BitMap(NumPhysPages);

    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    
    printf("numPages is %d\n", numPages);

    //ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    pageTable = new TranslationEntry[numPages];
    int tt = 0;
    for (i = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
        pageTable[i].physicalPage = -1;
        pageTable[i].valid = FALSE;
        /*int pp = machine->memoryMap->Find();
        if(pp == -1){
            pageTable[i].valid = FALSE;
            continue;
        }
        machine->pageBelong[pp] = currentThread->getTID();
        machine->pageLRUtime[pp] = tt++;
        
        pageTable[i].physicalPage = pp;
        printf("%s:Allocate physical page %d to virtual page %d\n",
               currentThread->getName(), pp, i);
        //machine->memoryMap->Mark(pp);
        progMap->Mark(pp);
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE;*/     // if the code segment was entirely on
					// a separate page, we could set its 
					// pages to be read-only
    }
    
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    //bzero(machine->mainMemory, size);

// then, copy in the code and data segments into memory
    /*if (noffH.code.size > 0) {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        executable->ReadAt(&(machine->mainMemory[noffH.code.virtualAddr]),
			noffH.code.size, noffH.code.inFileAddr);
    }
    if (noffH.initData.size > 0) {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        executable->ReadAt(&(machine->mainMemory[noffH.initData.virtualAddr]),
			noffH.initData.size, noffH.initData.inFileAddr);
    }*/
    //LoadByByte(executable);

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    delete pageTable;
    delete progMap;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() {
    for(int i=0; i<TLBSize; ++i){
        machine->tlb[i].valid = FALSE;
        machine->LRUtime[i] = 0;
    }
    
    for(int i=0; i<NumPhysPages; ++i){
        if(progMap->Test(i)){
            machine->pageBelong[i] = currentThread->getTID();
        }
    }
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

void AddrSpace::clearMap(){
    for(int i=0; i<NumPhysPages; ++i){
        if(progMap->Test(i)){
            machine->memoryMap->Clear(i);
            progMap->Clear(i);
            machine->pageBelong[i] = -1;
            printf("%s:physical page %d cleared..\n", currentThread->getName(), i);
        }
    }
    
    int numm = 0;
    for(int i=0; i<machine->swapNum; ++i){
        if(machine->swapArea[i].tid == currentThread->getTID()){
            machine->swapArea[i].tid = -1;
            machine->swapArea[i].vpn = -1;
            numm++;
        }
    }
    for(int i=1; i<machine->swapNum; ++i){
        int t = i;
        while(t!=0){
            if(machine->swapArea[t-1].tid == -1 && machine->swapArea[t].tid != -1){
                machine->swapArea[t-1] = machine->swapArea[t];
                t = t-1;
            }
            else
                break;
        }
        machine->swapArea[i].tid = -1;
        machine->swapArea[i].vpn = -1;
    }
    machine->swapNum -= numm;
}

void AddrSpace::LoadByByte(OpenFile *executable){
    NoffHeader noffH;
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
    //printf("code:%d and %d\n", noffH.code.inFileAddr, noffH.code.size);
    if(noffH.code.size > 0){
        int pos = noffH.code.inFileAddr;
        for(int i=0; i<noffH.code.size; ++i){
            unsigned int vpn = (unsigned)(noffH.code.virtualAddr+i)/PageSize;
            if(!pageTable[vpn].valid)
                continue;
            unsigned int offset = (unsigned)(noffH.code.virtualAddr+i)%PageSize;
            int physAddr = pageTable[vpn].physicalPage * PageSize + offset;
            executable->ReadAt(&(machine->mainMemory[physAddr]), 1, pos+i);
        }
    }
    if(noffH.initData.size > 0){
        int pos = noffH.initData.inFileAddr;
        for(int i=0; i<noffH.initData.size; ++i){
            unsigned int vpn = (unsigned)(noffH.initData.virtualAddr+i)/PageSize;
            if(!pageTable[vpn].valid)
                continue;
            unsigned int offset = (unsigned)(noffH.initData.virtualAddr+i)%PageSize;
            int physAddr = pageTable[vpn].physicalPage * PageSize + offset;
            executable->ReadAt(&(machine->mainMemory[physAddr]), 1, pos+i);
        }
    }
    //printf("initdata:%d and %d\n", noffH.initData.inFileAddr, noffH.initData.size);
    //printf("uninitdata:%d and %d\n", noffH.uninitData.inFileAddr, noffH.uninitData.size);
}

void AddrSpace::dealWithPageFault(){
    int t = -1;
    int virtAddr = machine->ReadRegister(BadVAddrReg);
    unsigned int vpn = (unsigned) virtAddr / PageSize;
    unsigned int offset = (unsigned) virtAddr % PageSize;
    int physAddr = -1;
    stats->numPageFaults++;
    //exist empty physical page
    for(int i=0; i<NumPhysPages; ++i){
        if(!machine->memoryMap->Test(i)){
            printf("exist empty physical page %d and take it\n", i);
            for(int j=0; j<NumPhysPages; ++j){
                if(machine->memoryMap->Test(j) && j!= i)
                    machine->pageLRUtime[j]++;
            }
            t = i;
            machine->memoryMap->Mark(t);
            break;
        }
    }
    //no empty page in mainMemory
    if(t == -1){
        t = machine->pageLRUReplace();
        printf("no empty physical page and replace physical page %d\n", t);
        //update origin thread
        int tid = machine->pageBelong[t];
        myThreads[tid]->space->progMap->Clear(t);
        
        NoffHeader noffH;
        OpenFile *executable1 = fileSystem->Open(myThreads[tid]->space->filename);
        executable1->ReadAt((char *)&noffH, sizeof(noffH), 0);
        if ((noffH.noffMagic != NOFFMAGIC) &&
            (WordToHost(noffH.noffMagic) == NOFFMAGIC))
            SwapHeader(&noffH);
        ASSERT(noffH.noffMagic == NOFFMAGIC);
        
        int vv = -1;
        for(int i=0; i<myThreads[tid]->space->numPages; ++i){
            if((myThreads[tid]->space->pageTable[i].valid) && (myThreads[tid]->space->pageTable[i].physicalPage == t)){
                vv = i;
                break;
            }
        }
        physAddr = t * PageSize;
        myThreads[tid]->space->pageTable[vv].valid = FALSE;
        myThreads[tid]->space->pageTable[vv].physicalPage = -1;
        if(tid == currentThread->getTID()){
            for(int i=0; i<TLBSize; ++i){
                if(machine->tlb[i].virtualPage == vv){
                    machine->tlb[i].valid = FALSE;
                    for(int j=0; j<TLBSize; ++j){
                        if(machine->tlb[j].valid && machine->LRUtime[j] > machine->LRUtime[i]){
                            machine->LRUtime[j]--;
                        }
                    }
                    machine->LRUtime[i] = 0;
                    break;
                }
            }
        }
        if(myThreads[tid]->space->pageTable[vv].dirty){
            printf("write back\n");
            
            int writeAddr = noffH.code.inFileAddr + vv * PageSize;
            executable1->WriteAt(&(machine->mainMemory[physAddr]), PageSize, writeAddr);
        }
        
        //put in swapArea
        int ff = 0;
        for(int i=0; i<machine->swapNum; ++i){
            if(machine->swapArea[i].tid == tid && machine->swapArea[i].vpn == vv){
                ff = 1;
                for(int j=0; j<PageSize; ++j){
                    machine->swapArea[i].page[j] = machine->mainMemory[physAddr+j];
                }
            }
        }
        
        if(ff == 0){
            int nn = machine->swapNum;
            machine->swapArea[nn].tid = tid;
            machine->swapArea[nn].vpn = vv;
            for(int j=0; j<PageSize; ++j){
                machine->swapArea[nn].page[j] = machine->mainMemory[physAddr+j];
            }
            machine->swapNum++;
        }
            
        
        delete executable1;
    }
    
    //now get the physical page t
    NoffHeader noffH;
    OpenFile *executable = fileSystem->Open(filename);
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) &&
        (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);
    
    physAddr = t * PageSize;
    
    //get from swapArea if swapArea has the page
    int fg = 0;
    for(int i=0; i<machine->swapNum; ++i){
        if(machine->swapArea[i].tid == currentThread->getTID()
           && machine->swapArea[i].vpn == vpn){
            fg = 1;
            for(int j=0; j<PageSize; ++j){
                machine->mainMemory[physAddr+j] = machine->swapArea[i].page[j];
            }
        }
    }
    
    if(fg == 0){
        //if(virtAddr < noffH.code.size + noffH.initData.size){
            int readAddr = noffH.code.inFileAddr + vpn * PageSize;
            executable->ReadAt(&(machine->mainMemory[physAddr]), PageSize, readAddr);
        //}
    
        /*if(virtAddr >= noffH.code.size + noffH.initData.size){
            for(int i=0; i<PageSize; ++i)
                machine->mainMemory[physAddr+i] = 0;
        }*/
    }
    
    delete executable;
    
    pageTable[vpn].physicalPage = t;
    pageTable[vpn].valid = TRUE;
    pageTable[vpn].use = FALSE;
    pageTable[vpn].readOnly = FALSE;
    pageTable[vpn].dirty = FALSE;
    machine->pageBelong[t] = currentThread->getTID();
    progMap->Mark(t);
}
