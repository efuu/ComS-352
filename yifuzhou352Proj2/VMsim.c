//
//  main.c
//  VMsim
//
//  Created by Yifu Zhou on 2016/11/29.
//  Copyright © 2016年 Yifu. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>

sem_t *blockR;
sem_t exist;
sem_t existPF;

int pageSize;
int pageNum;
int frameNum;
int threadNum;
int counter;
int counterPF;
int dataSize;

int tracer;

//def and save data from readfile to memory management
typedef struct ArrayList
{
    int logicalAddr;
    int ThreadID;
}ArrayList;
ArrayList* Al;

//def and save data from memory management to PageFault
typedef struct ArrayListPF
{
    int pageNumPF;
    int threadIDPF;
    int offset;
}ArrayListPF;
ArrayListPF* AlPF;

//def the page table
typedef struct pageT
{
    int pageIndex;
    int frameIndex;
    bool isVaild;
}pageT;
pageT** pageTable;

//def and save in physical address
typedef struct pageL
{
    int LogicAddrinPage;
    int timerRecord;
}pageL;
pageL *pageLRU;

int timer;

/*
 *init value, easy to track
 */
void init()
{
    // i
    for (int i=0; i<dataSize; i++)
    {
        Al[i].logicalAddr = -1;
        Al[i].ThreadID = -1;
        AlPF[i].pageNumPF = -1;
        AlPF[i].threadIDPF = -1;
    }
    //init the pageTable 2D array
    for (int i=0; i<threadNum; i++)
    {
        for (int j=0; j<pageNum; j++)
        {
            pageTable[i][j].isVaild = false;
            pageTable[i][j].frameIndex = -1;
            pageTable[i][j].pageIndex = -1;
        }
    }
}

/*
 *return the page number when input the logical Address
 */
int getPageNum(int logicalA)
{
    int result = logicalA / pageSize;
    return result;
}

/*
 *return the offset when input the logical Address
 */
int getOffSet(int logicalA)
{
    int result = logicalA%pageSize;
    return result;
}

/*
 *count the totall vaild data in files, easy to limit the loop later
 */
void countTotalData(int i)
{
    char *trace = "trace_";
    char *txt = ".txt";
    char *c;
    c = malloc(20);
    sprintf(c,"%d",i);
    
    char filename[50];
    filename[0]=0;
    
    strcat(filename,trace);
    strcat(filename,c);
    strcat(filename,txt);
    
    FILE *file = fopen ( filename, "r" );
    
    if (file != NULL)
    {
        char line [100];
        // read each line
        while(fgets(line,sizeof line,file)!= NULL)
        {
            int selecter = atoi(line);
            if (selecter/pageSize>=pageNum)
            {
                break;
            }
            dataSize++;
        }
        fclose(file);
    }
    else
    {
        perror(filename);
    }
}

/*
 * the algorithm of LUR, return the chosen frame number
 */
int getFrameFromLUR()
{
    int resultTime = 999;
    int min = 999;
    for (int i=0; i<frameNum; i++)
    {
        if (pageLRU[i].timerRecord<resultTime)
        {
            resultTime = pageLRU[i].timerRecord;
            min = i;
        }
    }
    return min;
}

/*
 *function when create the process thread, read the data and send to Al list
 */
void *readData(void *i)
{
    counter = 0;
    char *trace = "trace_";
    char *txt = ".txt";
    char *c;
    c = malloc(20);
    sprintf(c,"%d",i);
    int idex = i-1;
    
    char filename[50];
    filename[0]=0;
    
    strcat(filename,trace);
    strcat(filename,c);
    strcat(filename,txt);
    
    FILE *file = fopen ( filename, "r" );
    
    if (file != NULL)
    {
        char line [100];
        //read each line
        while(fgets(line,sizeof line,file)!= NULL)
        {
            //when the logical address is oversize, break the while loop
            if (atoi(line) / pageSize>=pageNum)
            {
                printf("[process %d] address %d is invalid and so the user process terminates\n", i-1, atoi(line));
                break;
            }
            Al[counter].logicalAddr = atoi(line);
            Al[counter].ThreadID = i-1;
            counter++;
            sem_post(&exist);
            sem_wait(&blockR[idex]);
        }
        
        fclose(file);
    }
    
    else
    {
        perror(filename);
    }
}

/*
 * function when create the memory manager thread, to adjust whether the data is valid or not and send to AlPF list.
 */
void *memoryControl(void * v)
{
    int point2List = 0;
    while (point2List != dataSize)
    {
        sem_wait(&exist);
        int matchPage = getPageNum(Al[point2List].logicalAddr);
        int matchOffset = getOffSet(Al[point2List].logicalAddr);

            //adjust whether it is valid
            int currentThrID =Al[point2List].ThreadID;
            if (!pageTable[currentThrID][matchPage].isVaild)
            {
                AlPF[counterPF].pageNumPF = matchPage;
                AlPF[counterPF].threadIDPF = currentThrID;
                AlPF[counterPF].offset = matchOffset;
                counterPF++;
                
                printf("[Process %d] accesses address %d (page number = %d, page offset = %d) not in main memory.\n", currentThrID, Al[point2List].logicalAddr, matchPage, matchOffset);
                
                sem_post(&existPF);
            }
            else
            {
                printf("[Process %d] accesses address %d (page number = %d, page offset=%d) in main memory (frame number = %d).\n", currentThrID, Al[point2List].logicalAddr, matchPage, matchOffset, pageTable[currentThrID][matchPage].frameIndex);
                tracer++;
                pageLRU[pageTable[currentThrID][matchPage].frameIndex].timerRecord = timer;
                timer++;
                pageLRU[pageTable[currentThrID][matchPage].frameIndex].LogicAddrinPage = Al[point2List].logicalAddr;
                sem_post(&blockR[currentThrID]);
                
            }
        point2List++;
    
    }
}

/*
 * page fault function
 */
void *faultControl(void *v)
{
    int point2PF = 0;
    int currentFrameNum = 0;
    int hasFreeFrame = true;
    int frameNumMark2Output = 0;
    timer = 0;
    while(tracer!=dataSize)
    {
        sem_wait(&existPF);
        if (hasFreeFrame)
        {
            printf("[Process %d] finds a free frame in main memory (frame number = %d).\n", AlPF[point2PF].threadIDPF, currentFrameNum);
            printf("[Process %d] issues an I/O operation to swap in demanded page (page number = %d).\n", AlPF[point2PF].threadIDPF, AlPF[point2PF].pageNumPF);
            usleep(1000);
            
            pageTable[AlPF[point2PF].threadIDPF][AlPF[point2PF].pageNumPF].frameIndex = currentFrameNum;
            pageTable[AlPF[point2PF].threadIDPF][AlPF[point2PF].pageNumPF].pageIndex = AlPF[point2PF].pageNumPF;
            
            printf("[Process %d] demanded page (page number = %d) has been swapped in main memory (frame number = %d).\n", AlPF[point2PF].threadIDPF, AlPF[point2PF].pageNumPF, currentFrameNum);

            pageLRU[currentFrameNum].LogicAddrinPage = AlPF[point2PF].pageNumPF*pageSize+AlPF[point2PF].offset;
            pageLRU[currentFrameNum].timerRecord = timer;
            timer++;
            pageTable[AlPF[point2PF].threadIDPF][AlPF[point2PF].pageNumPF].isVaild = true;
            

            frameNumMark2Output = currentFrameNum;
            currentFrameNum++;
            if (currentFrameNum/frameNum == 0)
                hasFreeFrame = true;
            else
                hasFreeFrame = false;
        }
        else
        {
            currentFrameNum = getFrameFromLUR();
            frameNumMark2Output = currentFrameNum;
            printf("[Process %d] replaces a frame (frame number = %d) from the main memory.\n", AlPF[point2PF].threadIDPF, currentFrameNum);
            printf("[Process %d] issues an I/O operation to swap in demanded page (page number = %d).\n", AlPF[point2PF].threadIDPF, AlPF[point2PF].pageNumPF);
            usleep(1000);
            
            pageTable[AlPF[point2PF].threadIDPF][AlPF[point2PF].pageNumPF].frameIndex = currentFrameNum;
            pageTable[AlPF[point2PF].threadIDPF][AlPF[point2PF].pageNumPF].pageIndex = AlPF[point2PF].pageNumPF;
            
            printf("[Process %d] demanded page (page number = %d) has been swapped in main memory (frame number = %d).\n", AlPF[point2PF].threadIDPF, AlPF[point2PF].pageNumPF, currentFrameNum);
            
            pageLRU[currentFrameNum].LogicAddrinPage = AlPF[point2PF].pageNumPF*pageSize+AlPF[point2PF].offset;
            pageLRU[currentFrameNum].timerRecord = timer;
            timer++;
            pageTable[AlPF[point2PF].threadIDPF][AlPF[point2PF].pageNumPF].isVaild = true;

        }
        
        sem_post(&blockR[AlPF[point2PF].threadIDPF]);
        
        printf("[Process %d] accesses address %d (page number = %d, page offset=%d) in main memory (frame number = %d).\n", AlPF[point2PF].threadIDPF, pageLRU[frameNumMark2Output].LogicAddrinPage, AlPF[point2PF].pageNumPF, AlPF[point2PF].offset, frameNumMark2Output);
        point2PF++;
        tracer++;
    }
}

int main(int argc, char const *argv[])
{
    
    pageSize = atoi(argv[1]);
    pageNum = atoi(argv[2]);
    frameNum = atoi(argv[3]);
    threadNum = atoi(argv[4]);
    //printf("%d,%d,%d,%d\n",pageSize,pageNum,frameNum,threadNum);
    
    //malloc the 2D array
    pageTable = (pageT**)malloc(sizeof(pageT*)*threadNum);
    for (int i=0; i<threadNum; i++)
    {
        pageTable[i] = (pageT *)malloc(sizeof(pageT)*pageNum);
    }
    
    pageLRU = (pageL*)malloc(sizeof(pageL)*frameNum);
    
    //init sem
    sem_init(&exist, 0, 0);
    sem_init(&existPF, 0, 0);
    blockR = (sem_t*)malloc(sizeof(sem_t)*threadNum);
    for (int i=0; i<threadNum; i++)
    {
        sem_init(&blockR[i], 0, 0);
    }
    
    //init the value needed
    init();
    counter = 0;
    counterPF = 0;
    dataSize = 0;
    tracer = 0;
    
    //get totally data first, save in dataSize
    for (int i =1; i<=threadNum; i++)
    {
        countTotalData(i);
    }
    //init the Al and AlPF
    Al = (ArrayList*)malloc(sizeof(ArrayList)*dataSize);
    AlPF = (ArrayListPF*)malloc(sizeof(ArrayListPF)*dataSize);
    
    pthread_t thr[threadNum];
    for (int i =0;i<threadNum;i++)
    {
        pthread_create(&thr[i], NULL, readData, (void*)(i+1));
    }
    
    //create a memoryManager
    pthread_t memoryManage;
    pthread_create(&memoryManage, NULL, memoryControl, NULL);

    
    //create pageHandling thread
    pthread_t pageHandle;
    pthread_create(&pageHandle, NULL, faultControl, NULL);
    
    for(int j = 0;j<threadNum;j++)
    {
        pthread_join(thr[j],NULL);
    }
    
    pthread_exit(NULL);
}

