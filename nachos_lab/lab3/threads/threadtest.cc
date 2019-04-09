// threadtest.cc
//    Simple test case for the threads assignment.
//
//    Create two threads, and have them context switch
//    back and forth between themselves by calling Thread::Yield,
//    to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"
//#include "elevatortest.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
//     Loop 5 times, yielding the CPU to another ready thread
//    each iteration.
//
//    "which" is simply a number identifying the thread, for debugging
//    purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
        printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

void moreSimpleThread(int which){
    printf("Thread %d is running now!\n", currentThread->getTID());
    TS();
    currentThread->Finish();
}
void myFinish(int which){
    currentThread->Finish();
}

void myScheTest(int which){
    printf("Thread %d is running, priority is %d\n", currentThread->getTID(),
           currentThread->getPriority());
    if(which == 0)
        return;
    Thread* son = new Thread("1", 1);
    printf("Create Son, priority is %d\n", son->getPriority());
    son->Fork(myScheTest, 0);
    printf("Thread %d Yield\n", currentThread->getTID());
    currentThread->Yield();
}

//----------------------------------------------------------------------
// ThreadTest1
//     Set up a ping-pong between two threads, by forking a thread
//    to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");
    
    Thread *t = new Thread("forked thread");
    
    t->Fork(SimpleThread, 1);
    SimpleThread(0);
}

void ThreadTest2(){
    DEBUG('t', "Entering ThreadTest2");
    
    //test the creation of threads
    printf("TEST 1:\n");
    Thread* tmp[135];
    for(int i=0; i<133; ++i){
        tmp[i] = new Thread("forked thread");
        //tmp[i]->Fork(moreSimpleThread, i);
    }
    printf("TEST 1 FINISHED\n\n");
}

void ThreadTest3(){
    DEBUG('t', "Entering ThreadTest3");
    printf("TS TEST:\n");
    //TS
    Thread* tmp[8];
    Thread* tmp1[8];
    for(int i=0; i<5; ++i){
        tmp[i] = new Thread("forked thread", 1);
        tmp[i]->setUID(2);
        tmp[i]->Fork(moreSimpleThread, i);
    }
    currentThread->Yield();
    //currentThread->myPrint();
    printf("TS TEST FINISHED\n\n");
    
    printf("FINSH TEST:\n");
    for(int i=0; i<5; ++i){
        tmp1[i] = new Thread("forked thread");
        tmp[i]->setUID(2);
        tmp1[i]->Fork(myFinish, i);
    }
    currentThread->Yield();
    TS();
    printf("Threads now:%d\n", threadNum);
    printf("FINISH TEST FINISHED\n\n");
}

void ThreadTest4(){
    DEBUG('t', "Entering ThreadTest4");
    printf("Enterting ThreadTest4!\n");
    printf("Create threads:\n");
    Thread* tmp[13];
    for(int i=0; i<5; ++i){
        tmp[i] = new Thread("1", 1);
        tmp[i]->setUID(2);
        printf("TID is %d and priority is %d\n", tmp[i]->getTID(), tmp[i]->getPriority());
    }
    for(int i=0; i<5; ++i){
        tmp[i]->Fork(myScheTest, 1);
    }
    
    currentThread->Yield();
}

//----------------------------------------------------------------------
// Producer and Consumer
//     use Semaphore and Condition
//----------------------------------------------------------------------
Semaphore* mutex = new Semaphore("mutex", 1);
Semaphore* empty = new Semaphore("empty", 2);
Semaphore* full = new Semaphore("full", 0);
void producer(int which){
    int num = 5;
    while(num--){
        printf("producer is trying to insert an item..\n");
        empty->P();
        mutex->P();
        printf("producer successfully insert an item!\n");
        mutex->V();
        full->V();
    }
}
void consumer(int which){
    int num = 5;
    while(num--){
        printf("consumer is trying to remove an item..\n");
        full->P();
        mutex->P();
        printf("consumer successfully remove an item!\n");
        mutex->V();
        empty->V();
    }
}

void ThreadTest6(){
    DEBUG('t', "Entering ThreadTest6");
    Thread* p = new Thread("producer");
    Thread* c = new Thread("consumer");
    c->Fork(consumer, 1);
    p->Fork(producer, 1);
    currentThread->Yield();
}
//--------------------------------------------------------------------//
int buffer = 0;
Condition* conp = new Condition("producer1");
Condition* conc = new Condition("consumer1");
Lock* lock = new Lock("lock1");
void producer1(int which){
    int num = 5;
    while(num--){
        printf("producer1 is trying to insert an item..\n");
        lock->Acquire();
        while(buffer == 2){
            conp->Wait(lock);
        }
        buffer++;
        printf("producer1 successfully insert an item!\n");
        conc->Signal(lock);
        lock->Release();
    }
}
void consumer1(int which){
    int num = 5;
    while(num--){
        printf("consumer1 is trying to remove an item..\n");
        lock->Acquire();
        while(buffer == 0){
            conc->Wait(lock);
        }
        buffer--;
        printf("consumer1 successfully remove an item!\n");
        conp->Signal(lock);
        lock->Release();
    }
}

void ThreadTest7(){
    DEBUG('t', "Entering ThreadTest7");
    Thread* p = new Thread("producer1");
    Thread* c = new Thread("consumer1");
    c->Fork(consumer1, 1);
    p->Fork(producer1, 1);
    currentThread->Yield();
}
//----------------------------------------------------------------------
// Barrier
//     Invoke a test routine.
//----------------------------------------------------------------------
Barrier* barrier = new Barrier(3);

void barrierTest(int which){
    printf("Thread %d is trying to go through...Now are %d threads\n", currentThread->getTID(), barrier->cnt + 1);
    barrier->barrierFunc();
    printf("Thread %d going througn!\n", currentThread->getTID());
}

void ThreadTest8(){
    Thread* tmp[12];
    for(int i=0; i<11; ++i){
        tmp[i] = new Thread("barrierThread");
        tmp[i]->Fork(barrierTest, 1);
    }
    currentThread->Yield();
}
//----------------------------------------------------------------------
// read/write lock
//     Invoke a test routine.
//----------------------------------------------------------------------
readwriteLock* rwlock = new readwriteLock();

void reader1(int which){
    printf("Thread %d is goint to read!\n",currentThread->getTID());
    rwlock->reader();
    printf("Thread %d finish reading!\n",currentThread->getTID());
}

void writer1(int which){
    printf("Thread %d is goint to write!\n",currentThread->getTID());
    rwlock->writer();
    printf("Thread %d finish writing!\n",currentThread->getTID());
}

void ThreadTest9(){
    Thread* tmp[10];
    for(int i=0; i<8; ++i){
        tmp[i] = new Thread("1");
    }
    for(int i=0; i<3; ++i)
        tmp[i]->Fork(reader1, 1);
    tmp[3]->Fork(writer1, 1);
    for(int i=4; i<8; ++i)
        tmp[i]->Fork(reader1,1);
    currentThread->Yield();
}
//----------------------------------------------------------------------
// ThreadTest
//     Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
        case 1:
            ThreadTest1();
            break;
        case 2:
            ThreadTest2();
            break;
        case 3:
            ThreadTest3();
            break;
        case 4:
            ThreadTest4();
            break;
        case 6:
            ThreadTest6();
            break;
        case 7:
            ThreadTest7();
            break;
        case 8:
            ThreadTest8();
            break;
        case 9:
            ThreadTest9();
            break;
        default:
            printf("No test specified.\n");
            break;
    }
}

