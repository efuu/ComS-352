//
//  uthread.c
//  
//
//  Created by Yifu Zhou on 16/10/25.
//
//

#include "uthread.h"
/* rewrite the gettid() that can return system call.*/
pid_t gettid()
{
    return syscall(SYS_gettid); 
}

/*  
 It will be called before any other functions of the uthread library can be called. It initializes the uthread system and specifies the maximum number of kernel threads to be argument numKernelThreads.
 */
void uthread_init(int numKernelThreads)
{
    KTsize = numKernelThreads;
    NumKT = 1;
    sizeofQueue = 0;
    inMainTH = 0;
    previousSlot = 0;
    kth = (struct kernel_info*)malloc(numKernelThreads);
    /* init the kernel thread table*/
    for (int i = 0; i<numKernelThreads; i++) {
        kth[i].ID = 0;
        kth[i].startTime = 0;
        kth[i].previousSlot = 0;
    }
    /* get the start time of this process*/
    gettimeofday(&tv1, NULL);
    stInProcess = (double)(tv1.tv_usec /1000000) + (double) (tv1.tv_sec);
}
/*
 Return the minimun slot of user thread. 
 It includes the property if two thread are in the same slot, the first in will be return.
 */
struct thread_info_queue *findMinSlot()
{
    struct thread_info_queue *q;
    for (int i=sizeofQueue-2; i>=0; i--) {
        if (queue[sizeofQueue-1]->slot>=queue[i]->slot)
        {
            q = queue[i];
            queue[i] = queue[sizeofQueue-1];
            queue[sizeofQueue-1] = q;
        }
    }
    sizeofQueue--;
    return queue[sizeofQueue];
}
/*
 If less than numKernelThreads kernel threads have been active, a new kernel thread is created by clone() to execute function func(); otherwise, a context of a new user-level thread should be properly created and stored on the priority ready queue.
 */
int uthread_create(void(*func)())
{
    if (NumKT++<KTsize)
    {
        void *child_stack;
        child_stack = (void*)malloc(4000); child_stack+=4000;
        
//        printf("In clone(), first: ID: %d; KEY: %d; startTime: %f\n",gettid(), key, st);
        clone(func, child_stack, CLONE_VM|CLONE_FILES, NULL);
    }
    else
    {
        //construct a thread record
        struct thread_info_queue *th;
        th=(struct thread_info_queue *)malloc(sizeof(struct thread_info_queue));
        th->ucp=(ucontext_t *)malloc(sizeof(ucontext_t));
        getcontext(th->ucp); //initialize the context structure
        th->ucp->uc_stack.ss_sp=(void *)malloc(16384);
        th->ucp->uc_stack.ss_size=16384;
        
        th->slot = 0;
        queue[sizeofQueue] = th;
        sizeofQueue++;
        makecontext(th->ucp, func, 0); //make the context for a thread running func
    }
}
/*
 The calling thread requests to yield the kernel thread to another user-level thread with the same or higher priority.
 */
void uthread_yield()
{
//    printf("(In the yield()):Thread id : gettid() == %d\n", gettid());
    int _ID = gettid();
    int key = -1;
    /*
     For here, if the table ID is 0, means it is still a initial form, so once it meets, I would say it is the new kernel thread first run into yield, and the start time must be stInProcess. Then I can keep recording the data.
     */
    for (int i = 0; i<KTsize; i++)
    {
        if (kth[i].ID == 0) {
            kth[i].ID = _ID;
            key = i;
            inMainTH = 1;
            break;
        }
        if (kth[i].ID ==_ID )
        {
            key = i;
            break;
        }
    }
    struct thread_info_queue *th;
    th=(struct thread_info_queue *)malloc(sizeof(struct thread_info_queue));
    th->ucp=(ucontext_t *)malloc(sizeof(ucontext_t));
    gettimeofday(&tv2, NULL);
    double et;
    et = (double)(tv2.tv_usec /1000000) + (double) (tv2.tv_sec);
    /* start to record the kernel thread table data*/
    if (inMainTH ==1)
    {
        th->slot = et - stInProcess + kth[key].previousSlot;
        inMainTH = 0;
    }
    else
       th->slot = et - kth[key].startTime + kth[key].previousSlot;
//    printf("In yield the kth[KEY].startTime :%f \n", kth[key].startTime);
//    printf("ID: %d;KEY: %d; slot: %f \n", gettid(), key, th->slot);
    
    sizeofQueue++;
    queue[sizeofQueue-1] = th;
    
    struct thread_info_queue *th1;
    th1 = findMinSlot();
    
    gettimeofday(&tv1, NULL);
    double st;
    st = (double)(tv1.tv_usec /1000000) + (double) (tv1.tv_sec);
    kth[key].startTime = st;
    kth[key].previousSlot = th1->slot;
    //printf("before swap, the KTH info: StartTime: %f, previousSlot: %f \n", st, th1->slot);
    //tv1 = tv2;
    //th1->startTime = et;
    swapcontext(th->ucp, th1->ucp);
}

/*
 If no ready user-level thread in the system, the whole process terminates; otherwise, a ready user thread with the highest priority should be mapped to the kernel thread to run.
 */
void uthread_exit()
{
    if(sizeofQueue == 0) exit(0); //all threads are finished
    
    struct thread_info_queue *th;
    th = findMinSlot();
    int _ID = gettid();
    int key = -1;
    /* here is for init the main() kernel thread.*/
    for (int i = 0; i<KTsize; i++)
    {
        if (kth[i].ID == 0) {
            kth[i].ID = _ID;
            key = i;
            break;
        }
        if (kth[i].ID ==_ID )
        {
            key = i;
            break;
        }
    }
    gettimeofday(&tv1, NULL);
    double st;
    st = (double)(tv1.tv_usec /1000000) + (double) (tv1.tv_sec);
    kth[key].startTime = st;
    kth[key].previousSlot = 0;
//    printf("In exit(), first: ID: %d; KEY: %d; startTime: %f\n",gettid(), key, st);
    setcontext(th->ucp);
}