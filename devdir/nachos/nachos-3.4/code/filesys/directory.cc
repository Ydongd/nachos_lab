// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory(int size)
{
    table = new DirectoryEntry[size];
    tableSize = size;
    for (int i = 0; i < tableSize; i++)
	table[i].inUse = FALSE;
}

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
    delete [] table;
} 

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file)
{
    (void) file->ReadAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file)
{
    (void) file->WriteAt((char *)table, tableSize * sizeof(DirectoryEntry), 0);
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
    /*for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
	    return i;
    return -1;		// name not in directory*/
    OpenFile* nameFile = new OpenFile(2);
    int filelen = nameFile->hdr->FileLength();
    char *buffer = new char[filelen+1];
    nameFile->ReadAt(buffer, filelen, 0);
    
    for(int i=0; i<tableSize; ++i){
        if(table[i].inUse){
            char *tname = new char[table[i].length + 1];
            for(int j=0; j<table[i].length; ++j){
                tname[j] = buffer[table[i].offset + j];
            }
            tname[table[i].length] = '\0';
            if(strcmp(name, tname) == 0)
                return i;
        }
    }
    return -1;
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    int i = FindIndex(name);

    if (i != -1)
	return table[i].sector;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector, bool type)
{ 
    if (FindIndex(name) != -1)
	return FALSE;

    for (int i = 0; i < tableSize; i++)
        if (!table[i].inUse) {
            table[i].inUse = TRUE;
            //strncpy(table[i].name, name, FileNameMaxLen);
            table[i].sector = newSector;
            table[i].type = type;
            
            OpenFile* nameFile = new OpenFile(2);
            nameFile->setPosition(nameFile->hdr->FileLength());
            int len = strlen(name);
            table[i].offset = nameFile->hdr->FileLength();
            table[i].length = len;
            char* buffer = new char[len+1];
            for(int j=0; j<len; ++j){
                buffer[j] = name[j];
            }
            buffer[len] = '\0';
            nameFile->Write(buffer, strlen(name));
            
            
        return TRUE;
	}
    return FALSE;	// no space.  Fix when we have extensible files.
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{ 
    int i = FindIndex(name);

    if (i == -1)
	return FALSE; 		// name not in directory
    table[i].inUse = FALSE;
    return TRUE;	
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List()
{
    OpenFile* nameFile = new OpenFile(2);
    int filelen = nameFile->hdr->FileLength();
    char *buffer = new char[filelen+1];
    nameFile->ReadAt(buffer, filelen, 0);
    
   for (int i = 0; i < tableSize; i++)
       if (table[i].inUse){
           //printf("%s\n", table[i].name);
           char *tname = new char[table[i].length + 1];
           for(int j=0; j<table[i].length; ++j){
               tname[j] = buffer[table[i].offset + j];
           }
           tname[table[i].length] = '\0';
           printf("%s\n", tname);
       }
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print()
{ 
    FileHeader *hdr = new FileHeader;
    OpenFile* nameFile = new OpenFile(2);
    int filelen = nameFile->hdr->FileLength();
    char *buffer = new char[filelen+1];
    nameFile->ReadAt(buffer, filelen, 0);
    
    printf("Directory contents:\n");
    for (int i = 1; i < tableSize; i++)
	if (table[i].inUse) {
        char *tname = new char[table[i].length + 1];
        for(int j=0; j<table[i].length; ++j){
            tname[j] = buffer[table[i].offset + j];
        }
        tname[table[i].length] = '\0';
	    printf("Name: %s, Sector: %d\n", tname, table[i].sector);
	    hdr->FetchFrom(table[i].sector);
	    hdr->Print();
	}
    printf("\n");
    delete hdr;
}

void Directory::PrintPath(int sector){
    FileHeader *hdr = new FileHeader;
    OpenFile* nameFile = new OpenFile(2);
    int filelen = nameFile->hdr->FileLength();
    char *buffer = new char[filelen+1];
    nameFile->ReadAt(buffer, filelen, 0);
    
    int len = table[0].length;
    char *tname;
    
    for(int i=0; i<tableSize; ++i){
        if(table[i].inUse && table[i].sector == sector){
            len += table[i].length;
            tname = new char[len + 2];
            int j = 0;
            for(int k=0; k<table[0].length; ++k, ++j){
                tname[j] = buffer[table[0].offset + k];
            }
            tname[j] = '/';
            j++;
            for(int k=0; k<table[i].length; ++k, ++j){
                tname[j] = buffer[table[i].offset + k];
            }
            tname[j] = '\0';
            printf("File Path: %s\n", tname);
        }
    }
}
