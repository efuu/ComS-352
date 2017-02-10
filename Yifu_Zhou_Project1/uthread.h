//
//  uthread.hpp
//  
//
//  Created by Yifu Zhou on 16/10/25.
//
//

#ifndef uthread_hpp
#define uthread_hpp

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/time.h>
#include <sys/syscall.h>

void uthread_init(int numKernelThreads);
int uthread_create(void(*func)());
void uthread_yield();
void uthread_exit();
void th1();
void th2();
void th3();
pid_t gettid();
int KTsize;
int NumKT;
int sizeofQueue;
/*record the start time in this process*/
double stInProcess;
/*record the previous slot mapping time*/
double previousSlot;
int inMainTH; //0 is false, 1 is true
/*structure in recording the time*/
struct timeval tv1, tv2;

/* the queue I build, I use it in Array*/
struct thread_info_queue{
    ucontext_t *ucp;
    double slot;

};
struct thread_info_queue *queue[50];

/* the structure table I used to save Kernel ID, mapping to the current user thread's start time, and its previous slot.*/
struct kernel_info{
    int ID;
    double startTime;
    double previousSlot;
};struct kernel_info *kth;

#endif /* uthread_hpp */
