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

#include "disk.h"
#include "bitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known 
// sectors, so that they can be located on boot-up.
#define FreeMapSector 		0
#define DirectorySector 	1

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
    //printf("initialize\n");
    //mutex = new Semaphore("mutex", 1);
    DEBUG('f', "Initializing the file system.\n");
    if (format) {

        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
	    FileHeader *mapHdr = new FileHeader;
	    FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");
    // First, allocate space for FileHeaders for the directory and bitmap
    // (make sure no one else grabs these!)
	freeMap->Mark(FreeMapSector);	    
	freeMap->Mark(DirectorySector);
    // Second, allocate space for the data blocks containing the contents
    // of the directory and bitmap files.  There better be enough space!
	ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize, 1));
    ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize, 1));

    // Flush the bitmap and directory FileHeaders back to disk
    // We need to do this before we can "Open" the file, since open
    // reads the file header off of disk (and currently the disk has garbage
    // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
    mapHdr->lastVisitTime = time(NULL);
    dirHdr->lastVisitTime = time(NULL);
    mapHdr->lastWriteTime = time(NULL);
    dirHdr->lastWriteTime = time(NULL);
	mapHdr->WriteBack(FreeMapSector);    
	dirHdr->WriteBack(DirectorySector);
   // printf("dir file type:%d\n", dirHdr->fileType);
    //printf("map file type:%d\n", mapHdr->fileType);

    // OK to open the bitmap and directory files now
    // The file system operations assume these two files are left open
    // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
     
    // Once we have the files "open", we can write the initial version
    // of each file back to disk.  The directory at this point is completely
    // empty; but the bitmap has been changed to reflect the fact that
    // sectors on the disk have been allocated for the file headers and
    // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
	freeMap->WriteBack(freeMapFile);	 // flush changes to disk
    directory->WriteBack(directoryFile);
    	if (DebugIsEnabled('f')) {
    	    freeMap->Print();
    	    directory->Print();

            delete freeMap; 
        	delete directory; 
        	delete mapHdr; 
        	delete dirHdr;
    	}
    } else {
    // if we are not formatting the disk, just open the files representing
    // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
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
FileSystem::Create(char *Name, int initialSize, int fileType)
{
    //printf("root addr:%x\n", directoryFile);
    char * name = new char[strlen(Name)];
    strcpy(name, Name);
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    OpenFile *currDirectoryFile;
   // FileHeader *dirHdr = new FileHeader;
    //FileHeader *mapHdr = new FileHeader;
    int sector;
    bool success;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

   
    getFileName(name, directory, currDirectoryFile);
    //printf("name:%s\n", name);
    //directory->Print();
    /*
    freeMap = new BitMap(NumSectors);
    printf("1\n");
    freeMapFile = new OpenFile(0);
    printf("2\n");
    freeMap->FetchFrom(freeMapFile);
    printf("3\n");
    freeMap->Print();
    printf("sector: %d\n", directory->Find(name));
    */
    if (directory->Find(name) != -1)
      success = FALSE;			// file is already in directory
    else {
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);
        //freeMap->Print();
        sector = freeMap->Find();	// find a sector to hold the file header
        if (sector == -1) 		
            success = FALSE;		// no free block for file header 
        else if (!directory->Add(name, sector))
            success = FALSE;	// no space in directory
	    else {
    	    hdr = new FileHeader;
            //printf("try to allocate\n");
	        if (!hdr->Allocate(freeMap, initialSize, fileType))
            	success = FALSE;	// no space on disk for data
	        else {
           // printf("everything worked\n");	
	    	    success = TRUE;
		    // everthing worked, flush all changes back to disk
    	    	hdr->WriteBack(sector);
                 
                //if we create a directory
                if(fileType == 1)		
                {
                    //printf("initialize the directory\n");
                    //first open this file
                    OpenFile *newDirectoryFile = new OpenFile(sector);
                    //then initialize a directory
                    Directory * newDirectory = new Directory(NumDirEntries);
                    //write back to the file
                    newDirectory->WriteBack(newDirectoryFile);

                    delete newDirectoryFile;
                    delete newDirectory;
                    
                }
               // printf("directoryFile sector\n");
                //directory->List();
               // directory->Print();
               // OpenFile *rootFile = new OpenFile(1);
                //printf("root primtable:%d\n",( currDirectoryFile->hdr->primaryIndexTable)[0]);
                //Directory *root = new Directory(NumDirEntries);
                //delete rootFile;
                //printf("curDir:%x\n", currDirectoryFile);
    	    	directory->WriteBack(currDirectoryFile);
                directory->FetchFrom(currDirectoryFile);
                directory->List();
    	    	freeMap->WriteBack(freeMapFile);
               // directory->Print();
	        }
            delete hdr;
	    }
        delete freeMap;
        //delete mapHdr;
    }
    delete currDirectoryFile;
    delete [] name;
    delete directory;
    //delete dirHdr;
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
FileSystem::Open(char *Name)
{ 
    //printf("--------------open----------------\n");
    char *name = new char[strlen(Name)];
    strcpy(name, Name);
    //Directory *directory = new Directory(NumDirEntries);
    Directory *directory;
    OpenFile *openFile = NULL;
    int sector;

    //printf("Opening file: %s\n", name);

    getFileName(name, directory,openFile);
    DEBUG('f', "Opening file %s\n", name);
    //directory->FetchFrom(directoryFile);
   // printf("-------name:%s\n", name);
    sector = directory->Find(name); 
    if (sector >= 0) 
    {
        //mutex->P();
        FileHeader *fileHdr = new FileHeader;
        fileHdr->FetchFrom(sector);
        fileHdr->numVisits ++;
        fileHdr->WriteBack(sector);
    	openFile = new OpenFile(sector);	// name was found in directory 
        //mutex->V();
    }
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
FileSystem::Remove(char *Name)
{ 
    char *name = new char[strlen(Name)];
    strcpy(name, Name);
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    OpenFile *openFile;
    int sector;
    
   // printf("delete name:%s\n",name);
    getFileName(name, directory, openFile);
    //directory->List();
    sector = directory->Find(name);
    //printf("file sector:%d\n", sector);
    if (sector == -1) {
       delete directory;
       return FALSE;             // file not found 
    }
    /*
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    //printf("file sector:%d\n", sector);
    if (sector == -1) {
       delete directory;
       return FALSE;			 // file not found 
    }
    */
    //mutex->P();
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    //is file
    if(fileHdr->fileType == 0)
    {
        printf("file visit num:%d\n", fileHdr->numVisits);
        
        if(fileHdr->numVisits != 1)
        {
            fileHdr->numVisits --;
            fileHdr->WriteBack(sector);
            printf("remove failed\n");
            
            return FALSE;
            //mutex->V();
        }
        else
        {
            fileHdr->numVisits --;
            fileHdr->WriteBack(sector);
            
        
            freeMap = new BitMap(NumSectors);
            freeMap->FetchFrom(freeMapFile);

            fileHdr->Deallocate(freeMap);  		// remove data blocks
            freeMap->Clear(sector);			// remove header block

            directory->Remove(name);
           // printf("2\n");
            freeMap->WriteBack(freeMapFile);		// flush to disk

            directory->WriteBack(openFile);        // flush to disk
           printf("remove success!\n");
        }
    }
    //is directory
    else 
    {
       // mutex->V();
        //printf("3\n");
        OpenFile *dirFile = new OpenFile(sector);
        Directory *dir = new Directory(NumDirEntries);
        dir->FetchFrom(dirFile);
        for (int i = 0; i < dir->tableSize; i++)
        {
            if ((dir->table)[i].inUse)
            {
                char *newName = new char[100];
                strcpy(newName, Name);
                strcat(newName, "/");
                strcat(newName, (dir->table)[i].name);
                //printf("ty to delete:%s\n", newName);
                Remove(newName);
            }
           
        }
        freeMap = new BitMap(NumSectors);
        freeMap->FetchFrom(freeMapFile);

        fileHdr->Deallocate(freeMap);       // remove data blocks
        freeMap->Clear(sector);         // remove header block

        directory->Remove(name);
        freeMap->WriteBack(freeMapFile);        // flush to disk

        directory->WriteBack(openFile);        // flush to disk
        delete dirFile;
        delete dir;
        /*debug
        printf("direcotry content:\n");
        directory->List();
        printf("-------\n");
        printf("---bitmap---\n");
        
        printf("-------\n");
        */

    }
    freeMap->Print();
    if(openFile != NULL)
        delete openFile;
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
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    printf("-----------------------------------------\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();
    printf("-----------------------------------------\n");
    printf("Directory file header:\n");
    printf("-----------------------------------------\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();
    printf("-----------------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();
    printf("-----------------------------------------\n");
    directory->FetchFrom(directoryFile);
    directory->Print();
    printf("-----------------------------------------\n");
    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
} 



void
FileSystem::getFileName(char *&name, Directory *& directory, OpenFile *&currDirectoryFile)
{
    OpenFile *rootFile = new OpenFile(1);//because we finally delete currDirectoryFile, so we donot need to delete rootFile
    currDirectoryFile = rootFile;
    char *fname = new char[strlen(name)];//father directory name
    char *nextPath = new char[strlen(name)];//next path eg. A/B
    char *temp_name = new char[strlen(name)];//temp name
    directory = new Directory(NumDirEntries);
    while(1)
    {
        directory->FetchFrom(currDirectoryFile); 
        //directory->Print();
        
        //handle path   root/A/B -> root/ + A/ + B
        
        int tokNum = 0;
        int i = 0;
        int fstart = 0;//index of fname
        int sstart = 0;//index of next path
       // printf("name:%s\n", name);
        while(tokNum < 2 && i < strlen(name))
        {
            //printf("tokNum:%d i:%d\n", tokNum, i);
            if(name[i] == '/')
            {
                tokNum ++;
                if(tokNum == 1) fstart = i + 1;
                if(tokNum == 2) sstart = i + 1;
            }

            i ++;
        }
       // printf("fstart:%d sstart:%d\n", fstart, sstart);
        //printf("fstart:%d sstart:%d\n", fstart, sstart);
        //no more directory, current file name is what we want to create

        if(sstart == 0)
        {
            sstart = strlen(name) + 1;
            nextPath = fname;
            for(int j = fstart; j < sstart; j ++)
            {
                fname[j - fstart] = name[j];
                if(j == sstart - 1)
                    fname[j - fstart] = '\0';
            }
            //printf("fname:%s\n", fname);
            //printf("name:%s\n", name);

            for(int k = 0; k < strlen(fname); k ++)
            {
                name[k] = fname[k];
            }
            name[strlen(fname)] = '\0';

            break;
        }
        for(int j = fstart; j < sstart; j ++)
        {
            fname[j - fstart] = name[j];
            if(j == sstart - 1)
                fname[j - fstart] = '\0';
        }
       
        for(int j = sstart; j < strlen(name); j ++)
        {
            nextPath[j - sstart] = name[j];
            if(j == strlen(name) - 1)
                nextPath[j - sstart + 1] = '\0';
        }

        for(int j = fstart; j < strlen(name); j ++)
        {
            temp_name[j - fstart] = name[j];
            if(j == strlen(name) - 1)
                temp_name[j - fstart + 1] = '\0';
        }
        //printf("fname:%s len:%d, nextPath:%s len:%d\n", fname, strlen(fname),nextPath, strlen(nextPath));   


        if(directory->Find(fname) == -1)
        {
            //directory->Print();
           // printf("fname:%s name=null\n", fname);
            name = NULL;            //no such directory
            break;
        }
        int newSector = directory->Find(fname);//new Sector is the hdr of next directory
       // printf("newSector:%d\n", newSector);
        currDirectoryFile = new OpenFile(newSector);
        strcpy(name,temp_name);// new path
      //  printf("new path:%s\n", name);
        
   }
  // printf("get out of getFileName name:%s\n", name);
      //  delete directoryFile2;
    delete []fname;
    //delete rootFile;
    if(fname != nextPath)
        delete []nextPath;
    delete [] temp_name;
   //return name;
}