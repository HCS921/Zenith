#ifndef STORAGE_CACHE_H
#define STORAGE_CACHE_H

#include <windows.h>

#define MAX_CACHE_ENTRIES 65536

typedef struct {
    u64 PathHash;
    u64 Size;
} cache_entry;

typedef struct {
    cache_entry Entries[MAX_CACHE_ENTRIES];
    u32 Count;
    b32 Modified;
} storage_cache;

storage_cache GlobalCache = {0};
CRITICAL_SECTION CacheLock;

// FNV-1a Hash
internal u64
HashString(char *Str)
{
    u64 Hash = 14695981039346656037ULL;
    while(*Str) {
        Hash ^= (unsigned char)*Str++;
        Hash *= 1099511628211ULL;
    }
    return Hash;
}

// Wide char version
internal u64
HashStringW(wchar_t *Str)
{
    u64 Hash = 14695981039346656037ULL;
    while(*Str) {
        // Hash bytes of wchar
        u16 C = (u16)*Str++;
        Hash ^= (C & 0xFF);
        Hash *= 1099511628211ULL;
        Hash ^= (C >> 8);
        Hash *= 1099511628211ULL;
    }
    return Hash;
}

void
InitCache()
{
    InitializeCriticalSection(&CacheLock);
    
    // Try load from file securely in User Space
    HANDLE File = CreateFileW(GlobalAppData_Cache, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if(File != INVALID_HANDLE_VALUE) {
        DWORD Read;
        ReadFile(File, &GlobalCache, sizeof(storage_cache), &Read, 0);
        CloseHandle(File);
    }
    GlobalCache.Modified = false;
}

void
SaveCache()
{
    if(!GlobalCache.Modified) return;
    
    HANDLE File = CreateFileW(GlobalAppData_Cache, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(File != INVALID_HANDLE_VALUE) {
        DWORD Written;
        WriteFile(File, &GlobalCache, sizeof(storage_cache), &Written, 0);
        CloseHandle(File);
    }
    GlobalCache.Modified = false;
}

u64
GetCachedSize(wchar_t *Path)
{
    EnterCriticalSection(&CacheLock);
    
    u64 Hash = HashStringW(Path);
    u32 Index = Hash % MAX_CACHE_ENTRIES;
    u32 Start = Index;
    
    u64 Result = (u64)-1;
    while(GlobalCache.Entries[Index].PathHash != 0) {
        if(GlobalCache.Entries[Index].PathHash == Hash) {
            Result = GlobalCache.Entries[Index].Size;
            break;
        }
        Index = (Index + 1) % MAX_CACHE_ENTRIES;
        if(Index == Start) break;
    }
    
    LeaveCriticalSection(&CacheLock);
    return Result;
}

void
SetCachedSize(wchar_t *Path, u64 Size)
{
    EnterCriticalSection(&CacheLock);
    
    u64 Hash = HashStringW(Path);
    u32 Index = Hash % MAX_CACHE_ENTRIES;
    u32 Start = Index;
    
    while(GlobalCache.Entries[Index].PathHash != 0) {
        if(GlobalCache.Entries[Index].PathHash == Hash) {
            // Update
            GlobalCache.Entries[Index].Size = Size;
            GlobalCache.Modified = true;
            LeaveCriticalSection(&CacheLock);
            return;
        }
        Index = (Index + 1) % MAX_CACHE_ENTRIES;
        if(Index == Start) {
             LeaveCriticalSection(&CacheLock);
             return; // Full
        }
    }
    
    // New slot
    GlobalCache.Entries[Index].PathHash = Hash;
    GlobalCache.Entries[Index].Size = Size;
    GlobalCache.Modified = true;
    
    LeaveCriticalSection(&CacheLock);
}

b32
IsCacheModified()
{
    return GlobalCache.Modified;
}

#endif

