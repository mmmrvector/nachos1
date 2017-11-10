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
   	interrupt->Halt();
    }
	else if((which == PageFaultException))
	{
		
		machine->LRU();
	}
	
    else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}

void Machine::LRU()
{
		
		int virtAddr = machine->registers[BadVAddrReg];
		unsigned int vpn = (unsigned) virtAddr / PageSize;
		bool noPageLeft = TRUE;
		int temp_i = 0;
		//if there is page table not been used
		for(int i = 0; i < NumPhysPages; i ++)
		{
			if((machine->pageTable[i]).use == FALSE)
			{
				(machine->pageTable[i]).physicalPage = memBitMap->Find();
				//printf("physic :%d\n",(machine->pageTable[i]).physicalPage); 
				(machine->pageTable[i]).tid = currentThread->gettid();
				(machine->pageTable[i]).virtualPage = vpn;
				(machine->pageTable[i]).valid = TRUE;
				noPageLeft = FALSE;
				temp_i = i;
				break;
			}
		}

		//all the physical page have been used
		if(noPageLeft == TRUE)
		{
			printf("all the physical pages have been used\n");
			int temp = 2147483644;	

			for(int i = 0; i < NumPhysPages; i ++)
			{
				if(machine->pageLasttime[i] < temp)
				{
					temp = machine->pageLasttime[i];
					temp_i = i;
				}
			}
			//(machine->pageTable[temp_i]).physicalPage = temp_i;
			printf("virtual page %d is writen back to the disk and physical page %d is free\n", (machine->pageTable[temp_i]).virtualPage, (machine->pageTable[temp_i]).physicalPage);
			machine->disk->WriteAt(&(machine->mainMemory[(machine->pageTable[temp_i]).physicalPage*PageSize]), 128, (machine->pageTable[temp_i]).virtualPage*PageSize);
			(machine->pageTable[temp_i]).tid = currentThread->gettid();
			(machine->pageTable[temp_i]).virtualPage = vpn;
			(machine->pageTable[temp_i]).valid = TRUE;
		}

		printf("physical page %d is distributed to virtual page %d\n", (machine->pageTable[temp_i]).physicalPage, vpn);
		machine->disk->ReadAt(&(machine->mainMemory[(machine->pageTable[temp_i]).physicalPage*PageSize]), 128, vpn*PageSize);
		//machine->diskPos += 128;
}
