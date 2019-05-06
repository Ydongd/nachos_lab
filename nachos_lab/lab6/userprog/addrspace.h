// addrspace.h 
//	Data structures to keep track of executing user programs 
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
  public:
    AddrSpace(AddrSpace *space);
    
    AddrSpace(OpenFile *executable);	// Create an address space,
					// initializing it with the program
					// stored in the file "executable"
    ~AddrSpace();			// De-allocate an address space

    void InitRegisters();		// Initialize user-level CPU registers,
					// before jumping to user code

    void SaveState();			// Save/restore address space-specific
    void RestoreState();		// info on a context switch 

    BitMap* progMap;
    void clearMap();
    void LoadByByte(OpenFile *executable);
    char* filename;
    void dealWithPageFault();
    void Suspend();
    void Resume(int t);
    
    void copyPageTable(TranslationEntry *to){
        for(int i = 0; i < numPages; i++){
            to[i].virtualPage = pageTable[i].virtualPage;
            to[i].physicalPage = pageTable[i].physicalPage;
            to[i].valid = pageTable[i].valid;
            to[i].use = pageTable[i].use;
            to[i].dirty = pageTable[i].dirty;
            to[i].readOnly = pageTable[i].readOnly;  // if the code segment was entirely on
        }
    }
    

    TranslationEntry *pageTable;	// Assume linear page table translation
					// for now!
    unsigned int numPages;		// Number of pages in the virtual 
					// address space
};

#endif // ADDRSPACE_H
