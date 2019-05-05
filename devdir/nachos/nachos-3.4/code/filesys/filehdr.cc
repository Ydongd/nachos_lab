// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{
    //printf("%d\n", NumDirect);
    //printf("?\n");
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space

    /*int block_begin = freeMap->MyFind(numSectors);
    if(block_begin != -1){
        for(int i=0; i<numSectors; ++i)
            dataSectors[i] = block_begin + i;
        return TRUE;
    }*/
    
    if(numSectors <= NumDirect){
        for (int i=0; i<numSectors; ++i)
            dataSectors[i] = freeMap->Find();
    }
    else{
        for(int i=0; i<NumDirect; ++i)
            dataSectors[i] = freeMap->Find();
        dataSectors[NumDirect] = freeMap->Find();
        int lastSectors = numSectors - NumDirect;
        int index[32];
        if(lastSectors<=32){
            for(int i=0; i<lastSectors; ++i)
                index[i] = freeMap->Find();
        }
        else{
            for(int i=0; i<32; ++i)
                index[i] = freeMap->Find();
        }
        synchDisk->WriteSector(dataSectors[NumDirect], (char*)index);
        lastSectors -= 32;
        if(lastSectors > 0){
            dataSectors[NumDirect+1] = freeMap->Find();
            int index1[32];
            for(int i=0; i<lastSectors; ++i)
                index1[i] = freeMap->Find();
            synchDisk->WriteSector(dataSectors[NumDirect+1], (char*)index1);
        }
    }
    //printf("?\n");
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    if(numSectors <= NumDirect){
        for (int i = 0; i < numSectors; i++) {
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors[i]);
        }
    }
    else{
        for(int i=0; i<NumDirect; ++i)
            freeMap->Clear((int) dataSectors[i]);
        int lastSectors = numSectors - NumDirect;
        char* index = new char[SectorSize];
        synchDisk->ReadSector(dataSectors[NumDirect], index);
        if(lastSectors <= 32){
            for(int i=0; i<lastSectors; ++i)
                freeMap->Clear((int)index[i*4]);
        }
        else{
            for(int i=0; i<32; ++i)
                freeMap->Clear((int)index[i*4]);
        }
        lastSectors -= 32;
        if(lastSectors > 0){
            char* index1 = new char[SectorSize];
            synchDisk->ReadSector(dataSectors[NumDirect+1], index1);
            for(int i=0; i<lastSectors; ++i)
                freeMap->Clear((int)index1[i*4]);
        }
        freeMap->Clear((int) dataSectors[NumDirect]);
        if(numSectors > NumDirect + 32)
            freeMap->Clear((int) dataSectors[NumDirect+1]);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    //return(dataSectors[offset / SectorSize]);
    int sector = offset/SectorSize;
    if(sector<NumDirect){
        //printf("0\n");
        return dataSectors[sector];
    }
    else if(sector >= NumDirect && sector < NumDirect+32){
        //printf("1\n");
        int index[32];
        synchDisk->ReadSector(dataSectors[NumDirect], (char*)index);
        return index[sector - NumDirect];
    }
    else if(sector >= NumDirect+32 && sector < NumDirect+64){
        //printf("2\n");
        int index[32];
        synchDisk->ReadSector(dataSectors[NumDirect+1], (char*)index);
        return index[sector - NumDirect-32];
    }
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];
    int flag = 0;

    printf("FileHeader contents.  File size: %d.  File blocks: ", numBytes);
    
    if(hdr_sector==0||hdr_sector==1||hdr_sector==2||hdr_sector==4)
        flag = 1;
    
    if(numSectors <= NumDirect){
        for (i = 0; i < numSectors; ++i){
            printf("%d ", dataSectors[i]);
            //if(hdr_sector==4||dataSectors[i]==5||dataSectors[i]==7||dataSectors[i]==9)
                //flag = 1;
            if(dataSectors[i]==8)
                flag = 1;
        }
    }
    else{
        for (i = 0; i < NumDirect; ++i){
            printf("%d ", dataSectors[i]);
            //if(dataSectors[i]==4||dataSectors[i]==5||dataSectors[i]==7||dataSectors[i]==9)
                //flag = 1;
            if(dataSectors[i] == 8)
                flag = 1;
        }
        int lastSectors = numSectors - NumDirect;
        int index[32];
        synchDisk->ReadSector(dataSectors[NumDirect], (char*)index);
        if(lastSectors<=32){
            for(int i=0; i<lastSectors; ++i)
                printf("%d ", index[i]);
        }
        else{
            for(int i=0; i<32; ++i)
                printf("%d ", index[i]);
        }
        lastSectors -= 32;
        if(lastSectors > 0){
            int index1[32];
            synchDisk->ReadSector(dataSectors[NumDirect+1], (char*)index1);
            for(int i=0; i<lastSectors; ++i)
                printf("%d ", index1[i]);
        }
    }
    printf("\n");
    
    if(flag == 0){
        OpenFile *curDirec = new OpenFile(3);
        Directory *dd = new Directory(10);
        dd->FetchFrom(curDirec);
        
        dd->PrintPath(hdr_sector);
    }
    
    if(flag == 0)
        printf("File Type: %s\n", type);

    
    if(flag == 0)
        printTime();
    
    printf("File contents:\n");
    if(numSectors <= NumDirect){
        for (i = k = 0; i < numSectors; i++) {
            synchDisk->ReadSector(dataSectors[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n");
        }
    }
    else{
        for (i = k = 0; i < NumDirect; i++) {
            synchDisk->ReadSector(dataSectors[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n");
        }
        int lastSectors = numSectors - NumDirect;
        int index[32];
        synchDisk->ReadSector(dataSectors[NumDirect], (char*)index);
        if(lastSectors<=32){
            for(i = k = 0; i < lastSectors; ++i){
                synchDisk->ReadSector(index[i], data);
                for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                        printf("%c", data[j]);
                    else
                        printf("\\%x", (unsigned char)data[j]);
                }
                printf("\n");
            }
        }
        else{
            for(i = k = 0; i < 32; ++i){
                synchDisk->ReadSector(index[i], data);
                for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                        printf("%c", data[j]);
                    else
                        printf("\\%x", (unsigned char)data[j]);
                }
                printf("\n");
            }
        }
        //printf("\n?\n");
        lastSectors -= 32;
        if(lastSectors > 0){
            int index1[32];
            synchDisk->ReadSector(dataSectors[NumDirect+1], (char*)index1);
            for(i = k = 0; i < lastSectors; ++i){
                synchDisk->ReadSector(index1[i], data);
                for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                        printf("%c", data[j]);
                    else
                        printf("\\%x", (unsigned char)data[j]);
                }
                printf("\n");
            }
        }
    }
    printf("\n");
    delete [] data;
}

bool FileHeader::Extend(BitMap *freeMap, int bytes){
    numBytes += bytes;
    int originNum = numSectors;
    numSectors = divRoundUp(numBytes, SectorSize);
    if(originNum == numSectors)
        return TRUE;
    else if(freeMap->NumClear() < numSectors - originNum)
        return FALSE;
    int needed = numSectors - originNum;
    if(numSectors <= NumDirect){
        for(int i=originNum; i<numSectors; ++i){
            dataSectors[i] = freeMap->Find();
            //printf("%d\n", dataSectors[i]);
            needed--;
        }
    }
    else{ //numSectors > NumDirect
        if(originNum<=NumDirect){
            dataSectors[NumDirect] = freeMap->Find();
            int index[32];
            int tn = needed;
            if(tn <= 32){
                for(int i=0; i<tn; ++i){
                    index[i] = freeMap->Find();
                    needed--;
                }
            }
            else{
                for(int i=0; i<32; ++i){
                    index[i] = freeMap->Find();
                    needed--;
                }
            }
            synchDisk->WriteSector(dataSectors[NumDirect], (char*)index);
            if(needed>0){
                dataSectors[NumDirect+1] = freeMap->Find();
                int index1[32];
                tn = needed;
                for(int i=0; i<tn; ++i){
                    index1[i] = freeMap->Find();
                    needed--;
                }
                synchDisk->WriteSector(dataSectors[NumDirect+1], (char*)index1);
            }
        }
        else if(originNum>NumDirect && originNum<=(NumDirect+32)){
            int index[32];
            synchDisk->ReadSector(dataSectors[NumDirect], (char*)index);
            int begin = originNum - NumDirect;
            int tn = needed;
            for(int i=0; i<tn; ++i){
                if(begin + i == 32){
                    break;
                }
                index[begin+i] = freeMap->Find();
                needed--;
            }
            synchDisk->WriteSector(dataSectors[NumDirect], (char*)index);
            if(needed > 0){
                int index1[32];
                dataSectors[NumDirect+1] = freeMap->Find();
                tn = needed;
                for(int i=0; i<tn; ++i){
                    index1[i] = freeMap->Find();
                    needed--;
                }
                synchDisk->WriteSector(dataSectors[NumDirect+1], (char*)index1);
            }
        }
        else if(originNum>(NumDirect+32) && originNum<=(NumDirect+64)){
            int index1[32];
            synchDisk->ReadSector(dataSectors[NumDirect+1], (char*)index1);
            int begin = originNum - NumDirect-32;
            int tn = needed;
            for(int i=0; i<tn; ++i){
                index1[begin+i] = freeMap->Find();
                needed--;
            }
            synchDisk->WriteSector(dataSectors[NumDirect+1], (char*)index1);
        }
    }
    
    return TRUE;
}
