// scheduler.cc
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would
//	end up calling FindNextToRun(), and that would put us in an
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

static int compare(Thread *t1, Thread *t2)
{
    if (t1->GetApproximateBurstTime() > t2->GetApproximateBurstTime())
        return 1;
    else if (t1->GetApproximateBurstTime() < t2->GetApproximateBurstTime())
        return -1;
    else
        return t1->getID() < t2->getID() ? -1 : 1;
    return 0;
}

Scheduler::Scheduler()
{
    readyList = new SortedList<Thread *>(compare);
    toBeDestroyed = NULL;
}

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{
    delete readyList;
}

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void Scheduler::ReadyToRun(Thread *thread)
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
    // cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    PutIntoQueue(thread);
    // readyList->Insert(thread);
    PreemptiveCheck(thread);
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun()
{
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    Thread *iterThread;
    ListIterator<Thread *> *iter;

    if (readyList->IsEmpty())
    {
        return NULL;
    }
    else
    {
        Thread *approximateThread = readyList->Front();
        // iter = new ListIterator<Thread *>(readyList);
        // for (; !iter->IsDone(); iter->Next())
        // {
        //     iterThread = iter->Item();
        //     if (iterThread->GetApproximateBurstTime() < approximateThread->GetApproximateBurstTime())
        //     {
        //         approximateThread = iterThread;
        //     }
        // }
        return RemoveFromQueue(approximateThread);
        // return readyList->RemoveFront();
    }
}

Thread *Scheduler::PutIntoQueue(Thread *newThread)
{
    readyList->Insert(newThread);
    DEBUG(dbgExpr, "[A] Tick [" << kernel->stats->totalTicks << "]: Thread [" << newThread->getID() << "] is inserted into queue");
    return newThread;
}

Thread *Scheduler::RemoveFromQueue(Thread *newThread)
{
    readyList->Remove(newThread);
    DEBUG(dbgExpr, "[B] Tick [" << kernel->stats->totalTicks << "]: Thread [" << newThread->getID() << "] is removed from queue");
    newThread->UpgradeTotalAgeTick();                        // Calculate remaining tick from last check point, and add back to thread's total age.
    newThread->SetAgeInitialTick(kernel->stats->totalTicks); // Keep current tick data to thread struct, it's useful when this thread is transfered in aging rather than go to execute.
    return newThread;
}

// void Scheduler::PreemptiveCheck(Thread *newThread) {
//     if (newThread->GetApproximateBurstTime() < kernel->currentThread->GetApproximateBurstTime())
//     {
//         // Ready for Preemptive
//         DEBUG(dbgExpr, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << kernel->currentThread->getID() << "] is now selected for execution, thread [" << newThread->getID() << "] is preempted, and it has executed [" << newThread->GetExecTick() << "] ticks");
//         kernel->interrupt->YieldOnReturn();
//     }
// }

void Scheduler::PreemptiveCheck(Thread *newThread)
{
    if (kernel->currentThread->getStatus() == RUNNING &&
        newThread->GetApproximateBurstTime() < kernel->currentThread->GetApproximateBurstTime())
    {
        DEBUG(dbgExpr, "[E] Tick [" << kernel->stats->totalTicks << "]: Thread [" << kernel->currentThread->getID() << "] is now selected for execution, thread [" << newThread->getID() << "] is preempted, and it has executed [" << newThread->GetExecTick() << "] ticks");
        // kernel->interrupt->YieldOnReturn();
        kernel->interrupt->OneTick(); // To simulate the time taken by the context switch
        kernel->currentThread->setStatus(READY);
        kernel->currentThread->preemtion = 1;
        PutIntoQueue(kernel->currentThread);
        Run(newThread, FALSE);
    }
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void Scheduler::Run(Thread *nextThread, bool finishing)
{
    Thread *oldThread = kernel->currentThread;

    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing)
    { // mark that we need to delete current thread
        ASSERT(toBeDestroyed == NULL);
        toBeDestroyed = oldThread;
    }

    if (oldThread->space != NULL)
    {                               // if this thread is a user program,
        oldThread->SaveUserState(); // save the user's CPU registers
        oldThread->space->SaveState();
    }

    oldThread->CheckOverflow(); // check if the old thread
                                // had an undetected stack overflow

    kernel->currentThread = nextThread; // switch to the next thread
    nextThread->setStatus(RUNNING);     // nextThread is now running

    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    DEBUG(dbgExpr, "[D] Tick [" << kernel->stats->totalTicks << "]: Thread [" << nextThread->getID() << "] is now selected for execution, thread [" << oldThread->getID() << "] starts IO, and it has executed [" << oldThread->GetExecTick() << "] ticks");
    nextThread->SetInitialTick(kernel->stats->totalTicks);

    // This is a machine-dependent assembly language routine defined
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread

    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed(); // check if thread we were running
                          // before this one has finished
                          // and needs to be cleaned up

    if (oldThread->space != NULL)
    {                                  // if there is an address space
        oldThread->RestoreUserState(); // to restore, do it.
        oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL)
    {
        delete toBeDestroyed;
        toBeDestroyed = NULL;
    }
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyList->Apply(ThreadPrint);
}
