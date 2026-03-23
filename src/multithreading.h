#ifndef MULTITHREADING_H_V2
#define MULTITHREADING_H_V2

#include "defs.h"
#include <windows.h>

#define MAX_QUEUE_ENTRIES 256

typedef void (*work_queue_callback)(void *Data);

typedef struct work_queue_entry
{
    work_queue_callback Callback;
    void *Data;
} work_queue_entry;

typedef struct work_queue
{
    work_queue_entry Entries[MAX_QUEUE_ENTRIES];
    volatile u32 EntryCount;
    volatile u32 NextEntryToWrite;
    volatile u32 NextEntryToRead;
    volatile u32 TotalJobsCount;
    volatile u32 CompletedJobsCount;
    
    HANDLE Semaphore;
} work_queue;

internal void InitializeWorkQueue(work_queue *Queue, int ThreadCount);
internal void AddWork(work_queue *Queue, work_queue_callback Callback, void *Data);

#endif // MULTITHREADING_H_V2
