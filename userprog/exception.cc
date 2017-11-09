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
		
		machine->tlbmiss ++;
		int virtAddr = machine->registers[BadVAddrReg];
		unsigned int vpn = (unsigned) virtAddr / PageSize;
		bool flag = false;
		//for(int  i = 0; i < TLBSize; i ++)
		//{
		//	if(machine->tlb[i].valid == FALSE)
		//	{
			int i = machine->flag;
			(machine->tlb[i]).virtualPage = vpn;
			(machine->tlb[i]).physicalPage =  (machine->pageTable[vpn]).physicalPage;
			printf("vpn:%d physicPage: %d\n", vpn, (machine->pageTable[vpn]).physicalPage);
			(machine->tlb[i]).valid = (machine->pageTable[vpn]).valid;
			(machine->tlb[i]).use =  (machine->pageTable[vpn]).use;
			(machine->tlb[i]).dirty =  (machine->pageTable[vpn]).dirty;
			(machine->tlb[i]).readOnly =  (machine->pageTable[vpn]).readOnly; 	
			
			machine->flag = (machine->flag + 1 )% 4;
			//flag = true;
			//break;
			//}
		//}
		/*
		if(!flag)
		{
			//int i = machine->flag;
			int temp = 2147483646;
			int temp_idx= 0;
			for(int i = 0; i < 4; i ++)
			{
				printf("lasttime: %d\n", (machine->lasttime)[i]);
				if((machine->lasttime)[i] < temp)
				{
					temp_idx = i;
					temp = (machine->lasttime)[i];
					//printf("time: %d\n", temp);
				}
			}
			(machine->tlb[temp_idx]).virtualPage = vpn;
			(machine->tlb[temp_idx]).physicalPage = vpn;
			(machine->tlb[temp_idx]).valid = 1;
			(machine->tlb[temp_idx]).use = FALSE;
			(machine->tlb[temp_idx]).dirty = FALSE;
			(machine->tlb[temp_idx]).readOnly = FALSE; 	
			//machine->flag = (machine->flag + 1 )% 4;
			//flag = true;
		}
		*/
		//ASSERT(FALSE);
	}
	
    else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
