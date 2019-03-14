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
        default:
            printf("No test specified.\n");
            break;
    }
}

