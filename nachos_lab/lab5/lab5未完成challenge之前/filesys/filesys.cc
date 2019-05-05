// filesys.cc 
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk 
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them 
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "time.h"
#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include "system.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1
#define NameSector          2
#define CurDirecSector      3

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number 
// of files that can be loaded onto the disk.
#define FreeMapFileSize 	(NumSectors / BitsInByte)
#define NumDirEntries 		10
#define DirectoryFileSize 	(sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).  
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{ 
    DEBUG('f', "Initializing the file system.\n");
    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
	FileHeader *mapHdr = new FileHeader;
	FileHeader *dirHdr = new FileHeader;
    FileHeader *namHdr = new FileHeader;
    FileHeader *curHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

    // First, allocate space for FileHeaders for the directory and bitmap
    // (make sure no one else grabs these!)
	freeMap->Mark(FreeMapSector);	    
	freeMap->Mark(DirectorySector);
    freeMap->Mark(NameSector);
    freeMap->Mark(CurDirecSector);

    // Second, allocate space for the data blocks containing the contents
    // of the directory and bitmap files.  There better be enough space!

	ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
	ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));
    ASSERT(namHdr->Allocate(freeMap, 0));
    ASSERT(curHdr->Allocate(freeMap, DirectoryFileSize));
    namHdr->hdr_sector = NameSector;
    curHdr->hdr_sector = 1;
    dirHdr->hdr_sector = DirectorySector;

    // Flush the bitmap and directory FileHeaders back to disk
    // We need to do this before we can "Open" the file, since open
    // reads the file header off of disk (and currently the disk has garbage
    // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
	mapHdr->WriteBack(FreeMapSector);    
	dirHdr->WriteBack(DirectorySector);
    namHdr->WriteBack(NameSector);
    curHdr->WriteBack(CurDirecSector);

    // OK to open the bitmap and directory files now
    // The file system operations assume these two files are left open
    // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        nameFile = new OpenFile(NameSector);
        curDirectoryFile = new OpenFile(CurDirecSector);
        //curDirectoryFile->hdr->hdr_sector = 1;
     
    // Once we have the files "open", we can write the initial version
    // of each file back to disk.  The directory at this point is completely
    // empty; but the bitmap has been changed to reflect the fact that
    // sectors on the disk have been allocated for the file headers and
    // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
	freeMap->WriteBack(freeMapFile);	 // flush changes to disk
    
        char buffer[] = "root";
        nameFile->Write(buffer, strlen(buffer));
        directory->table[0].inUse = TRUE;
        directory->table[0].offset = 0;
        directory->table[0].length = 4;
        directory->table[0].sector = 1;
        directory->WriteBack(directoryFile);
        directory->WriteBack(curDirectoryFile);
        
	if (DebugIsEnabled('f')) {
	    freeMap->Print();
	    directory->Print();

        delete freeMap; 
	delete directory; 
	delete mapHdr; 
	delete dirHdr;
    delete namHdr;
    delete curHdr;
	}
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
        nameFile = new OpenFile(NameSector);
        curDirectoryFile = new OpenFile(CurDirecSector);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk 
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file 
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool
FileSystem::Create(char *name, int initialSize)
{
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    directory = new Directory(NumDirEntries);
    //directory->FetchFrom(directoryFile);
    directory->FetchFrom(curDirectoryFile);

    if (directory->Find(name) != -1)
      success = FALSE;			// file is already in directory
    else {	
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();	// find a sector to hold the file header
    	if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        else if (!directory->Add(name, sector, FALSE))
            success = FALSE;	// no space in directory
	else {
    	    hdr = new FileHeader;
	    if (!hdr->Allocate(freeMap, initialSize))
            	success = FALSE;	// no space on disk for data
	    else {	
	    	success = TRUE;
		// everthing worked, flush all changes back to disk
            
            time_t timep;
            time(&timep);
            char *tmptime = asctime(gmtime((&timep)));
            hdr->setCreateTime(tmptime);
            hdr->setLastVisitTime(tmptime);
            hdr->setLastModifyTime(tmptime);
            
            hdr->setType(name);
            
            hdr->hdr_sector = sector;
            
    	    	hdr->WriteBack(sector); 		
    	    	//directory->WriteBack(directoryFile);
                directory->WriteBack(curDirectoryFile);
    	    	freeMap->WriteBack(freeMapFile);
	    }
            delete hdr;
	}
        delete freeMap;
    }
    delete directory;
    return success;
}

bool
FileSystem::CreateDir(char *name, int initialSize)
{
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;
    FileHeader *curHdr = new FileHeader;
    curHdr->FetchFrom(3);
    
    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);
    
    directory = new Directory(NumDirEntries);
    //directory->FetchFrom(directoryFile);
    directory->FetchFrom(curDirectoryFile);
    
    if (directory->Find(name) != -1)
        success = FALSE;            // file is already in directory
    else {
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        sector = freeMap->Find();    // find a sector to hold the file header
        if (sector == -1)
            success = FALSE;        // no free block for file header
        else if (!directory->Add(name, sector, TRUE))
            success = FALSE;    // no space in directory
        else {
            hdr = new FileHeader;
            initialSize = DirectoryFileSize;
            if (!hdr->Allocate(freeMap, initialSize))
                success = FALSE;    // no space on disk for data
            else {
                success = TRUE;
                // everthing worked, flush all changes back to disk
                
                time_t timep;
                time(&timep);
                char *tmptime = asctime(gmtime((&timep)));
                hdr->setCreateTime(tmptime);
                hdr->setLastVisitTime(tmptime);
                hdr->setLastModifyTime(tmptime);
                
                hdr->setType(name);
                
                hdr->hdr_sector = sector;
                
                hdr->WriteBack(sector);
                
                //if(type){
                    //Directory *directory = new Directory(NumDirEntries);
                    OpenFile* namFile = new OpenFile(2);
                    int filelen = namFile->hdr->FileLength();
                    char *buffer = new char[filelen+1];
                    namFile->ReadAt(buffer, filelen, 0);
                    
                    int ll = strlen(name);
                    int j;
                    char *tname = new char[ll + directory->table[0].length + 2];
                    for(j=0; j<directory->table[0].length; ++j){
                        tname[j] = buffer[directory->table[0].offset + j];
                    }
                    tname[j] = '/';
                    j++;
                    for(int i=0; i<ll; ++i, ++j){
                        tname[j] = name[i];
                    }
                    tname[j]='\0';
                    
                    Directory *dd = new Directory(NumDirEntries);
                    dd->table[0].inUse = TRUE;
                    namFile->setPosition(namFile->hdr->FileLength());
                    int len = strlen(tname);
                    dd->table[0].offset = namFile->hdr->FileLength();
                    dd->table[0].length = len;
                    dd->table[0].sector = curHdr->hdr_sector;
                
                    namFile->Write(tname, strlen(tname));
                    
                    OpenFile* fff = new OpenFile(sector);
                    dd->WriteBack(fff);
                //}
                curHdr->WriteBack(3);
                hdr->WriteBack(sector);
                //directory->WriteBack(directoryFile);
                directory->WriteBack(curDirectoryFile);
                freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
        }
        delete freeMap;
    }
    delete directory;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.  
//	To open a file:
//	  Find the location of the file's header, using the directory 
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *
FileSystem::Open(char *name)
{ 
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    DEBUG('f', "Opening file %s\n", name);
    //directory->FetchFrom(directoryFile);
    directory->FetchFrom(curDirectoryFile);
    sector = directory->Find(name); 
    if (sector >= 0) 		
	openFile = new OpenFile(sector);	// name was found in directory 
    delete directory;
    return openFile;				// return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool
FileSystem::Remove(char *name)
{ 
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;
    
    directory = new Directory(NumDirEntries);
    //directory->FetchFrom(directoryFile);
    directory->FetchFrom(curDirectoryFile);
    sector = directory->Find(name);
    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    
    if(synchDisk->openCnt[sector] > 0){
        printf("Cannot remove this file, %d openfiles still use it..\n",
               synchDisk->openCnt[sector]);
        return FALSE;
    }
    
    synchDisk->P(sector);
    
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap);  		// remove data blocks
    freeMap->Clear(sector);			// remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);		// flush to disk
    //directory->WriteBack(directoryFile);        // flush to disk
    directory->WriteBack(curDirectoryFile);
    
    synchDisk->V(sector);
    
    delete fileHdr;
    delete directory;
    delete freeMap;
    return TRUE;
} 

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void
FileSystem::List()
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    directory->List();
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void
FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    FileHeader *namHdr = new FileHeader;
    FileHeader *curHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();
    
    printf("Name file header:\n");
    namHdr->FetchFrom(NameSector);
    namHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();
    
    printf("Current Directory file header:\n");
    curHdr->FetchFrom(CurDirecSector);
    curHdr->Print();

    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();
    printf("\n");

    //directory->FetchFrom(directoryFile);
    directory->FetchFrom(curDirectoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete namHdr;
    delete freeMap;
    delete directory;
} 

void
FileSystem::Change(char *name){
    Directory *directory;
    int sector;
    FileHeader *curHdr = new FileHeader;
    curHdr->FetchFrom(3);
    
    directory = new Directory(NumDirEntries);
    //directory->FetchFrom(directoryFile);
    directory->FetchFrom(curDirectoryFile);
    
    int oriSector = curHdr->hdr_sector;
    OpenFile *ddd = new OpenFile(oriSector);
    directory->WriteBack(ddd);
    
    if(!strcmp(name, ".."))
        sector = directory->table[0].sector;
    else
        sector = directory->Find(name);
    
    OpenFile *dd = new OpenFile(sector);
    
    directory->FetchFrom(dd);
    directory->WriteBack(curDirectoryFile);
    curHdr->hdr_sector = sector;
    curHdr->WriteBack(3);
}
