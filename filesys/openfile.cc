// openfile.cc 
//	Routines to manage an open Nachos file.  As in UNIX, a
//	file must be open before we can read or write to it.
//	Once we're all done, we can close it (in Nachos, by deleting
//	the OpenFile data structure).
//
//	Also as in UNIX, for convenience, we keep the file header in
//	memory while the file is open.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "filehdr.h"
#include "openfile.h"
#include "system.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// OpenFile::OpenFile
// 	Open a Nachos file for reading and writing.  Bring the file header
//	into memory while the file is open.
//
//	"sector" -- the location on disk of the file header for this file
//----------------------------------------------------------------------

OpenFile::OpenFile(int sector)
{ 
    hdr = new FileHeader;
    synchDisk->rw_P(sector);
    hdr->FetchFrom(sector);
    hdrSectorNumber = sector;
    //printf("open sector:%d\n", sector);
    //printf("lastVisitTime addr:%x\n", &(hdr->lastVisitTime));
    //hdr->numVisits += 1;
    time(&(hdr->lastVisitTime));
    hdr->WriteBack(sector);
    synchDisk->rw_V(sector);
    //printf("lastVisitTime:%s\n", ctime(&(hdr->lastVisitTime)));
    seekPosition = 0;
}

//----------------------------------------------------------------------
// OpenFile::~OpenFile
// 	Close a Nachos file, de-allocating any in-memory data structures.
//----------------------------------------------------------------------

OpenFile::~OpenFile()
{
    delete hdr;
}

//----------------------------------------------------------------------
// OpenFile::Seek
// 	Change the current location within the open file -- the point at
//	which the next Read or Write will start from.
//
//	"position" -- the location within the file for the next Read/Write
//----------------------------------------------------------------------

void
OpenFile::Seek(int position)
{
    seekPosition = position;
}	

//----------------------------------------------------------------------
// OpenFile::Read/Write
// 	Read/write a portion of a file, starting from seekPosition.
//	Return the number of bytes actually written or read, and as a
//	side effect, increment the current position within the file.
//
//	Implemented using the more primitive ReadAt/WriteAt.
//
//	"into" -- the buffer to contain the data to be read from disk 
//	"from" -- the buffer containing the data to be written to disk 
//	"numBytes" -- the number of bytes to transfer
//----------------------------------------------------------------------

int
OpenFile::Read(char *into, int numBytes)
{
   int result = ReadAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

int
OpenFile::Write(char *into, int numBytes)
{
   int result = WriteAt(into, numBytes, seekPosition);
   seekPosition += result;
   return result;
}

//----------------------------------------------------------------------
// OpenFile::ReadAt/WriteAt
// 	Read/write a portion of a file, starting at "position".
//	Return the number of bytes actually written or read, but has
//	no side effects (except that Write modifies the file, of course).
//
//	There is no guarantee the request starts or ends on an even disk sector
//	boundary; however the disk only knows how to read/write a whole disk
//	sector at a time.  Thus:
//
//	For ReadAt:
//	   We read in all of the full or partial sectors that are part of the
//	   request, but we only copy the part we are interested in.
//	For WriteAt:
//	   We must first read in any sectors that will be partially written,
//	   so that we don't overwrite the unmodified portion.  We then copy
//	   in the data that will be modified, and write back all the full
//	   or partial sectors that are part of the request.
//
//	"into" -- the buffer to contain the data to be read from disk 
//	"from" -- the buffer containing the data to be written to disk 
//	"numBytes" -- the number of bytes to transfer
//	"position" -- the offset within the file of the first byte to be
//			read/written
//----------------------------------------------------------------------

int
OpenFile::ReadAt(char *into, int numBytes, int position)
{

    synchDisk->rw_P(hdrSectorNumber);
    hdr->FetchFrom(hdrSectorNumber);
    synchDisk->rw_V(hdrSectorNumber);
    //modify time
    time(&(hdr->lastVisitTime));
    int fileLength = hdr->FileLength();
    int i, firstSector, lastSector, numSectors;
    char *buf;

    if ((numBytes <= 0) || (position >= fileLength))
    	return 0; 				// check request
    if ((position + numBytes) > fileLength)		
	numBytes = fileLength - position;
    DEBUG('f', "Reading %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, fileLength);

    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    numSectors = 1 + lastSector - firstSector;


    synchDisk->arr_P();
    for(i = firstSector; i<= lastSector; i++)
    {
        synchDisk->rw_P(hdr->ByteToSector(i * SectorSize));
    }
    synchDisk->arr_V();

    // read in all the full and partial sectors that we need
    buf = new char[numSectors * SectorSize];
    for (i = firstSector; i <= lastSector; i++)	
    {
        //synchDisk->rw_P(hdr->ByteToSector(i * SectorSize));
        synchDisk->ReadSector(hdr->ByteToSector(i * SectorSize), 
					&buf[(i - firstSector) * SectorSize]);
        synchDisk->rw_V(hdr->ByteToSector(i * SectorSize));
    }

    // copy the part we want
    bcopy(&buf[position - (firstSector * SectorSize)], into, numBytes);
    

    delete [] buf;
    return numBytes;
}

int
OpenFile::WriteAt(char *from, int numBytes, int position)
{

    synchDisk->rw_P(hdrSectorNumber);
    hdr->FetchFrom(hdrSectorNumber);
    synchDisk->rw_V(hdrSectorNumber);

    //modify time
    time(&(hdr->lastVisitTime));
    time(&(hdr->lastWriteTime));
    //printf("lastVisitTime:%d  lastWriteTime:%d hdr->b2s:%d\n", hdr->lastVisitTime, hdr->lastWriteTime, hdr->ByteToSector(0));

    int fileLength = hdr->FileLength();
    int i, firstSector, lastSector, numSectors;
    bool firstAligned, lastAligned;
    char *buf;

    if ((numBytes <= 0))
	return 0;				// check request
    if ((position + numBytes) > fileLength)
    {
        int extraFileLength = numBytes + position - fileLength;
        int extraSectors = divRoundUp(extraFileLength, SectorSize);
        BitMap *freeMap = new BitMap(NumSectors);
        OpenFile *freeMapFile = new OpenFile(0);
        freeMap->FetchFrom(freeMapFile);
        if(!hdr->ExtendAllocate(freeMap, extraFileLength))
        {
            printf("extend allocate failed\n");
            return 0;
        }
        //need to write back header
       // printf("hdr sector number:%d\n", hdrSectorNumber);

        hdr->WriteBack(hdrSectorNumber);
        delete freeMap;
        delete freeMapFile;
    }
    DEBUG('f', "Writing %d bytes at %d, from file of length %d.\n", 	
			numBytes, position, fileLength);
   
    firstSector = divRoundDown(position, SectorSize);
    lastSector = divRoundDown(position + numBytes - 1, SectorSize);
    //printf("firstSector:%d lastSector:%d hdr->:%d\n", firstSector, lastSector, hdr->ByteToSector(0 * SectorSize));
    numSectors = 1 + lastSector - firstSector;

    buf = new char[numSectors * SectorSize];

    firstAligned = (position == (firstSector * SectorSize));
    lastAligned = ((position + numBytes) == ((lastSector + 1) * SectorSize));

// read in first and last sector, if they are to be partially modified
    if (!firstAligned)
        ReadAt(buf, SectorSize, firstSector * SectorSize);	
    if (!lastAligned && ((firstSector != lastSector) || firstAligned))
        ReadAt(&buf[(lastSector - firstSector) * SectorSize], 
				SectorSize, lastSector * SectorSize);	

// copy in the bytes we want to change 
    bcopy(from, &buf[position - (firstSector * SectorSize)], numBytes);

    synchDisk->arr_P();
    for(i = firstSector; i<= lastSector; i++)
    {
        synchDisk->rw_P(hdr->ByteToSector(i * SectorSize));
    }
    synchDisk->arr_V();
    for (i = firstSector; i <= lastSector; i++)	
    {
        //synchDisk->rw_P(hdr->ByteToSector(i * SectorSize));
        synchDisk->WriteSector(hdr->ByteToSector(i * SectorSize), 
					&buf[(i - firstSector) * SectorSize]);
       synchDisk->rw_V(hdr->ByteToSector(i * SectorSize));
    }
    //release visit header

    delete [] buf;
    return numBytes;
}

//----------------------------------------------------------------------
// OpenFile::Length
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
OpenFile::Length() 
{ 
    return hdr->FileLength(); 
}
