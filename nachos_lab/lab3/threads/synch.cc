// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) {
    mutex = new Semaphore(debugName, 1);
    name = debugName;
    holder = NULL;
}

Lock::~Lock() {
    delete mutex;
}

void Lock::Acquire() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    mutex->P();
    holder = currentThread;
    (void) interrupt->SetLevel(oldLevel);
}

void Lock::Release() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    mutex->V();
    holder = NULL;
    (void) interrupt->SetLevel(oldLevel);
}

bool Lock::isHeldByCurrentThread(){
    return currentThread == holder;
}

Condition::Condition(char* debugName) {
    name = debugName;
    waitList = new List;
}

Condition::~Condition() {
    delete waitList;
}

void Condition::Wait(Lock* conditionLock) {
    ASSERT(conditionLock->isHeldByCurrentThread() == TRUE);
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    waitList->Append((void*)currentThread);
    conditionLock->Release();
    currentThread->Sleep();
    conditionLock->Acquire();
    (void) interrupt->SetLevel(oldLevel);
}

void Condition::Signal(Lock* conditionLock) {
    ASSERT(conditionLock->isHeldByCurrentThread() == TRUE);
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    Thread* thread;
    thread = (Thread *)waitList->Remove();
    if (thread != NULL)
        scheduler->ReadyToRun(thread);
    (void) interrupt->SetLevel(oldLevel);
}

void Condition::Broadcast(Lock* conditionLock) {
    ASSERT(conditionLock->isHeldByCurrentThread() == TRUE);
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    Thread* thread;
    thread = (Thread *)waitList->Remove();
    while (thread != NULL){
        scheduler->ReadyToRun(thread);
        thread = (Thread *)waitList->Remove();
    }
    (void) interrupt->SetLevel(oldLevel);
}


Barrier::Barrier(int x){
    lock = new Lock("barrier lock");
    lackNum = new Condition("lack of threads");
    num = x;
    cnt = 0;
}

Barrier::~Barrier(){
    delete lock;
    delete lackNum;
}

void Barrier::barrierFunc(){
    lock->Acquire();
    cnt++;
    if(cnt < num){
        lackNum->Wait(lock);
    }
    else{
        cnt = 0;
        lackNum->Broadcast(lock);
    }
    lock->Release();
}

readwriteLock::readwriteLock(){
    write = new Semaphore("write", 1);
    mutex = new Semaphore("mutex", 1);
    rc = 0;
}

readwriteLock::~readwriteLock(){
    delete write;
    delete mutex;
}

void readwriteLock::reader(){
    mutex->P();
    rc++;
    if(rc == 1)
        write->P();
    mutex->V();
    printf("Thread %d is reading..\n", currentThread->getTID());
    currentThread->Yield();
    mutex->P();
    rc--;
    if(rc == 0)
        write->V();
    mutex->V();
}

void readwriteLock::writer(){
    write->P();
    printf("Thread %d is writing..\n", currentThread->getTID());
    write->V();
}




/*
 为何条件变量要和互斥量联合使用
 互斥锁一个明显的缺点是他只有两种状态：锁定和非锁定。而条件变量通过允许线程阻塞和等待另一个线程发送信号的方法弥补了互斥锁的不足，他常和互斥锁一起使用。
 使用时，条件变量被用来阻塞一个线程，当条件不满足时，线程往往解开相应的互斥锁并等待条件发生变化。一旦其他的某个线程改变了条件变量，他将通知相应的条件变量唤醒一个或多个正被此条件变量阻塞的线程。
 这些线程将重新锁定互斥锁并重新测试条件是否满足。一般说来，条件变量被用来进行线承间的同步。
 可以总结为：条件变量用在某个线程需要在某种条件才去保护它将要操作的临界区的情况下，从而避免了线程不断轮询检查该条件是否成立而降低效率的情况，这是实现了效率提高。。。
 
 In Thread1:
 
 pthread_mutex_lock(&m_mutex);
 pthread_cond_wait(&m_cond,&m_mutex);
 pthread_mutex_unlock(&m_mutex);
 
 
 
 In Thread2:
 
 pthread_mutex_lock(&m_mutex);
 pthread_cond_signal(&m_cond);
 pthread_mutex_unlock(&m_mutex);
 
 为什么要与pthread_mutex 一起使用呢？ 这是为了应对 线程1在调用pthread_cond_wait()但线程1还没有进入wait cond的状态的时候，此时线程2调用了 cond_singal 的情况。 如果不用mutex锁的话，这个cond_singal就丢失了。加了锁的情况是，线程2必须等到 mutex 被释放（也就是 pthread_cod_wait() 释放锁并进入wait_cond状态 ，此时线程2上锁） 的时候才能调用cond_singal.
 */

/*
 条件变量为什么要搭配互斥锁
 
 1 条件变量的改变一般是临界资源来完成的,那么修改临界资源首先应该加锁
 而线程在条件不满足的情况下要阻塞,等待别人唤醒,
 那么在阻塞后一定要把锁放开,等到合适的线程拿到锁去修改临界资源,否则会出现死锁
 
 2 在线程被唤醒后第一件事也应该是争取拿到锁,恢复以前加锁的状态.
 否则在执行条件变量成立后的代码也法保证其原子性
 
 所以条件变量和锁是相辅相成的:
 条件变量需要锁的保护
 锁需要 条件变量成立后,后重新上锁
 */
