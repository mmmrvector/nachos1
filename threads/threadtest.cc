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
//num
//the number of items
//----------------------------------------------------------------------
int num = 0;//max num = 10

//three semaphores
Semaphore *mutex1 = new Semaphore("mutex1", 1);
Semaphore *mutex2 = new Semaphore("mutex2", 1);
Semaphore *empty = new Semaphore("empty", 10);
Semaphore *full = new Semaphore("full", 0);


//producer  use P(), V()
void produce(int which)
{
	//while the buffer is empty
	empty->P();
	printf("%s%d try to get into the critical area\n", currentThread->getName(), which);
	//enter the critical area
	mutex1->P();
	printf("%s%d get into the critical area\n", currentThread->getName(), which);
	//switch
	currentThread->Yield();
	//produce the item
	num += 1;
	printf("%s%d produce an item, now the total number is %d\n", currentThread->getName(), which, num);
	//leave the critical area
	mutex1->V();
	//increase item
	full->V();
}

//consumer  use P(), V()
void consume(int which)
{
	
	//while the buffer is not empty
	full->P();
	printf("%s%d try to get into the critical area\n", currentThread->getName(), which);
	//enter the critical area
	mutex2->P();
	printf("%s%d get into the critical area\n", currentThread->getName(), which);
	//switch
	currentThread->Yield();
	//consume the item
	num -= 1;
	printf("%s%d consume an item, now the total number is %d\n", currentThread->getName(), which, num);
	//leave the critical area
	mutex2->V();
	//decrease item
	empty->V();
}

Lock *lock1 = new Lock("lock1");
Lock *lock2 = new Lock("lock2");
Condition *empty1 = new Condition("empty");
Condition *full1 = new Condition("full"); 

//producer use condition
void producer(int which)
{
	printf("%s%d try to get into the critical area\n", currentThread->getName(), which);
	lock1->Acquire();
	printf("%s%d get into the critical area\n", currentThread->getName(), which);
	currentThread->Yield();	
	//when the buffer is full
	while(num == 10)
	{
		empty1->Wait(lock1);
	}
	//produce an item
	num += 1;
	printf("%s%d produce an item, now the total number is %d\n", currentThread->getName(), which, num);
	lock1->Release();
	//wake up an consumer
	full1->Signal(lock2);
}

//consumer use conditoin
void consumer(int which)
{
	printf("%s%d try to get into the critical area\n", currentThread->getName(), which);
	lock2->Acquire();
	printf("%s%d get into the critical area\n", currentThread->getName(), which);
	//when the buffer is full
	currentThread->Yield();
	//when the buffer is empty
	while(num == 0)
	{
		full1->Wait(lock2);
	}
	//consume an item
	num -= 1;
	printf("%s%d consume an item, now the total number is %d\n", currentThread->getName(), which, num);
	lock2->Release();
	//wake up an producer
	empty1->Signal(lock1);
}
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
	for(int i = 0; i < 12; i ++)
	{
		if(which <= 2)
			produce(which);
		else
			consume(which);	
	}

}

//----------------------------------------------------------------------
//read/write lock
//----------------------------------------------------------------------
RWLock *rwlock = new RWLock("RWLock");
int data = 0;
void readThread(int which)
{	
	//acquire read lock
	rwlock->rdLock();
	printf("thread%d get into the critical area\n", which);
	printf("thread%d read the data: %d\n", which, data);
	//switch	
	if(which == 1)
	currentThread->Yield();
	printf("thread%d leave the critical area\n", which);
	//release read lock	
	rwlock->rdUnlock();
}

void writeThread(int which, int num)
{
	//acquire write lock
	rwlock->wrLock();
	printf("thread%d get into the critical area\n", which);
	data = num;
	printf("thead%d write the data: %d\n", which, data);
	currentThread->Yield();
	printf("thread%d leave the critical area\n", which);
	rwlock->wrUnlock();
}

void readWriteThread(int which)
{
	if(which == 1||which ==3||which == 5)
		readThread(which);
	else if(which == 2)
		writeThread(which, 2);
	else
		writeThread(which, 4);
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

    for(int i = 1; i <= 5; i ++)
	{
		printf("thread %d is created\n", i);
	    char *name = new char;
		name = "forked thread" + i;
		t[i] = new Thread("thread", i);
		t[i]->Fork(readWriteThread, i);		
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


