#ifndef MULTITHREADING_C
#define MULTITHREADING_C

#include "multithreading.h"

internal DWORD WINAPI WorkerThreadProc(LPVOID lpParameter);

internal DWORD WINAPI
WorkerThreadProc(LPVOID lpParameter)
{
    work_queue *Queue = (work_queue *)lpParameter;

    for(;;)
    {
        if(Queue->EntryCount > 0)
        {
            unsigned int OriginalNextEntryToRead = Queue->NextEntryToRead;
            unsigned int NewNextEntryToRead = (OriginalNextEntryToRead + 1) % MAX_QUEUE_ENTRIES;
            
            if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
            {
                unsigned int Index = InterlockedCompareExchange((volatile LONG *)&Queue->NextEntryToRead, NewNextEntryToRead, OriginalNextEntryToRead);
                if(Index == OriginalNextEntryToRead)
                {
                    work_queue_entry Entry = Queue->Entries[Index];
                    InterlockedDecrement((volatile LONG *)&Queue->EntryCount);
                    
                    Entry.Callback(Entry.Data);
                    
                    InterlockedIncrement((volatile LONG *)&Queue->CompletedJobsCount);
                    
                    if(Queue->EntryCount == 0 && Queue->CompletedJobsCount == Queue->TotalJobsCount)
                    {
                         Queue->TotalJobsCount = 0;
                         Queue->CompletedJobsCount = 0;
                    }
                }
            }
        }
        else
        {
            WaitForSingleObjectEx(Queue->Semaphore, INFINITE, FALSE);
        }
    }
    return 0;
}

internal void
InitializeWorkQueue(work_queue *Queue, int ThreadCount)
{
    Queue->EntryCount = 0;
    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;
    
    Queue->Semaphore = CreateSemaphoreExA(0, 0, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);
    
    for(int i = 0; i < ThreadCount; ++i)
    {
        CreateThread(0, 0, WorkerThreadProc, Queue, 0, 0);
    }
}

internal void
AddWork(work_queue *Queue, work_queue_callback Callback, void *Data)
{
    unsigned int NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % MAX_QUEUE_ENTRIES;
    
    work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    
    _WriteBarrier();
    
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    InterlockedIncrement((volatile LONG *)&Queue->TotalJobsCount);
    InterlockedIncrement((volatile LONG *)&Queue->EntryCount);
    
    ReleaseSemaphore(Queue->Semaphore, 1, 0);
}

#endif // MULTITHREADING_C

