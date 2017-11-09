// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"
#include "synch.h"
// testnum is set in main.cc
int testnum = 1;



//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
	for(int i = 0; i < 3; i ++)
	{
		printf("thread %d looped %d times\n", which, i);
		interrupt->OneTick();
	}

}


//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, (void*)1);

    SimpleThread(0);
}

//----------------------------------------------------------------------
//VectorTest
//----------------------------------------------------------------------
void
VectorTest()
{
	Thread *t[130];

    for(int i = 1; i <= 120; i ++)
	{
		printf("thread %d is created\n", i);
	    char *name = new char;
		name = "forked thread" + i;
		t[i] = new Thread("thread", i);
		t[i]->Fork(SimpleThread, i);		
	}
	//SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
    break;
	case 2:
	VectorTest();
	break;
    default:
	printf("No test specified.\n");
	break;
    }
}


