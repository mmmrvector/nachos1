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
#include <time.h>
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
FileHeader::Allocate(BitMap *freeMap, int fileSize, int _fileType)
{ 

    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    fileType = _fileType;
    createTime = time(NULL);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space
    /*
    for (int i = 0; i < numSectors; i++)
	dataSectors[i] = freeMap->Find();
    */
    int numPrimaryIndexTableEntries = divRoundUp(numSectors, 32);
    int tempNumSectors = numSectors;

    for(int i = 0; i < numPrimaryIndexTableEntries; i ++)
    {
        //allocate second-level index table
        primaryIndexTable[i] = freeMap->Find();
        int tempDataSectors[32];
        if(tempNumSectors < 32)
        {
            for(int j = 0; j < tempNumSectors; j ++)
                tempDataSectors[j] = freeMap->Find();
        }
        else
        {
            for(int j = 0; j < 32; j ++)
            {
                tempDataSectors[j] = freeMap->Find();
            }
            tempNumSectors -= 32;
        }
        synchDisk->WriteSector(primaryIndexTable[i], (char *)tempDataSectors);
    }

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
    //printf("get into Deallocate\n");
    //printf("numsectors:%d\n", numSectors);
    int tempNumSectors = numSectors;
    int numPrimaryIndexTableEntries = divRoundUp(numSectors,32);
    //printf("tempNumSectors:%d\n", tempNumSectors);
    //printf("numPrimaryIndexTableEntries:%d\n", numPrimaryIndexTableEntries);
    for (int i = 0; i < numPrimaryIndexTableEntries; i++) {
	ASSERT(freeMap->Test((int) primaryIndexTable[i]));  // ought to be marked!
    int tempDataSectors[32];
    synchDisk->ReadSector(primaryIndexTable[i], (char*)tempDataSectors);
    if(tempNumSectors < 32)
    {

        for(int j = 0; j < tempNumSectors; j ++)
        {
            freeMap->Clear((int)tempDataSectors[j]);
            //printf("dataSector:%d\n", tempDataSectors[j]);
        }
    }
    else
    {
        for(int j = 0; j < 32; j ++)
        {
            freeMap->Clear((int)tempDataSectors[j]);
        }
        tempNumSectors -= 32;
    }

    freeMap->Clear((int) primaryIndexTable[i]);
    }
    
    //printf("try to clear free map\n");
    //freeMap->Print();
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
    //printf("FetchFrom sector:%d\n", sector);
    synchDisk->ReadSector(sector, (char *)this);
   // printf("3\n");
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
    //which entry in primary index table
    int i = offset/(SectorSize*32);
    int i_offset = offset % (SectorSize*32);
    i_offset = i_offset / SectorSize;
    int tempDataSectors[32];
//    printf("primTable[%d]:%d\n",i, primaryIndexTable[i]);
    synchDisk->ReadSector(primaryIndexTable[i], (char*)tempDataSectors);
    return(tempDataSectors[i_offset]);
    //return(dataSectors[offset / SectorSize]);
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
    int tempDataSectors[32];
    char *data = new char[SectorSize];
    int numPrimaryIndexTableEntries = divRoundUp(numSectors, 32);
    int tempNumSectors = numSectors;

    printf("FileHeader contents.\nFile type: %d\nCreate time: %sLast visit time: %sLast modify time: %sFile size: %d\nFile blocks:\n", fileType, ctime(&(createTime)), ctime(&(lastVisitTime)), ctime(&(lastWriteTime)), numBytes);
    for (i = 0; i < numPrimaryIndexTableEntries; i++)
	printf("%d ", primaryIndexTable[i]);
    
    printf("\nFile contents:\n");


    for (i = k = 0; i < numPrimaryIndexTableEntries; i++) {
        synchDisk->ReadSector(primaryIndexTable[i], (char*)tempDataSectors);
        
        if(tempNumSectors < 32)
        {
            for(int m = 0; m < tempNumSectors; m ++)
            {
        	    synchDisk->ReadSector(tempDataSectors[m], data);
                for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
        	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
        		   printf("%c", data[j]);
                else
        		   printf("\\%x", (unsigned char)data[j]);
        	   }

            
            }
        }
        else
        {
            for(int m = 0; m < 32; m ++)
            {
                synchDisk->ReadSector(tempDataSectors[m], data);
                for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
               }

            }
        }
        printf("\n"); 
    }
    delete [] data;

}
