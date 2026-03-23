
#include <stdio.h> // For serialization
#include "multithreading.h" // For memory fence / atomic access if needed, or just Windows

// --- Definitions ---
// (Moved to filesystem.h)

// search_index defined in filesystem.h

search_index GlobalIndex = {0};

// Helper
internal b32
ContainsW(wchar_t *Haystack, wchar_t *Needle)
{
    if(!Needle[0]) return true;
    while(*Haystack)
    {
        wchar_t *H = Haystack;
        wchar_t *N = Needle;
        while(*H && *N)
        {
            wchar_t h = *H; if(h >= 'a' && h <= 'z') h -= 32;
            wchar_t n = *N; if(n >= 'a' && n <= 'z') n -= 32;
            if(h != n) break;
            H++; N++;
        }
        if(!*N) return true;
        Haystack++;
    }
    return false;
}

// --- Serialization ---

internal void
SaveIndexChunkToDisk()
{
    HANDLE File = CreateFileW(GlobalAppData_Index, FILE_APPEND_DATA, 0, 0, OPEN_ALWAYS, 0, 0);
    if(File != INVALID_HANDLE_VALUE)
    {
        DWORD Written;
        for(u32 i=0; i<GlobalIndex.Count; ++i)
        {
            index_entry *E = GlobalIndex.Entries + i;
            u32 PathLen = lstrlenW(E->FullPath);
            WriteFile(File, &PathLen, sizeof(u32), &Written, 0);
            WriteFile(File, E->FullPath, PathLen * sizeof(wchar_t), &Written, 0);
            WriteFile(File, &E->Size, sizeof(u64), &Written, 0);
            WriteFile(File, &E->LastWriteTime, sizeof(FILETIME), &Written, 0);
            WriteFile(File, &E->IsDirectory, sizeof(b32), &Written, 0);
        }
        CloseHandle(File);
    }
    GlobalIndex.TotalIndexedCount += GlobalIndex.Count;
    GlobalIndex.Count = 0;
    GlobalIndex.Arena.Used = 0;
}

internal b32
LoadIndexFromDisk()
{
    HANDLE File = CreateFileW(GlobalAppData_Index, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(File == INVALID_HANDLE_VALUE) return false;
    
    u64 Total = 0;
    DWORD Read;
    u32 PathLen = 0;
    while(ReadFile(File, &PathLen, sizeof(u32), &Read, 0) && Read == sizeof(u32))
    {
         SetFilePointer(File, PathLen * sizeof(wchar_t) + sizeof(u64) + sizeof(FILETIME) + sizeof(b32), 0, FILE_CURRENT);
         Total++;
    }
    GlobalIndex.TotalIndexedCount = Total;
    CloseHandle(File);
    GlobalIndex.IsReady = true;
    return true;
}

// --- Indexing Logic ---

internal void
RecursiveIndex(wchar_t *Directory)
{
    if(GlobalIndex.StopRequested) return;
    if(GlobalIndex.Count >= GlobalIndex.Capacity) return;

    wchar_t SearchPath[LONG_PATH];
    wchar_t NormPath[LONG_PATH];
    NormalizePathW(NormPath, Directory);
    lstrcpyW(SearchPath, NormPath);
    int Len = lstrlenW(SearchPath);
    if(Len > 0 && SearchPath[Len-1] != L'\\') SearchPath[Len++] = L'\\';
    SearchPath[Len++] = L'*';
    SearchPath[Len] = 0;
    
    WIN32_FIND_DATAW FindData;
    HANDLE FindHandle = FindFirstFileW(SearchPath, &FindData);
    
    if(FindHandle != INVALID_HANDLE_VALUE)
    {
        do 
        {
            if(GlobalIndex.StopRequested) break;
            if(wcscmp(FindData.cFileName, L".") == 0 || wcscmp(FindData.cFileName, L"..") == 0) continue;
            
            if(GlobalIndex.Count >= GlobalIndex.Capacity)
            {
                SaveIndexChunkToDisk();
            }
            
            u32 Slot = GlobalIndex.Count++;
            index_entry *E = GlobalIndex.Entries + Slot;
            
            wchar_t FullBuffer[LONG_PATH];
            lstrcpyW(FullBuffer, Directory);
            int DirLen = lstrlenW(FullBuffer);
            if(DirLen > 0 && FullBuffer[DirLen-1] != L'\\') FullBuffer[DirLen++] = L'\\';
            lstrcpyW(FullBuffer + DirLen, FindData.cFileName);
            
            E->FullPath = PushStringW(&GlobalIndex.Arena, FullBuffer);
            E->Size = ((u64)FindData.nFileSizeHigh << 32) | FindData.nFileSizeLow;
            E->LastWriteTime = FindData.ftLastWriteTime;
            E->IsDirectory = (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            
            if(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                b32 SkipDir = false;
                if(FindData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                    if(FindData.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT || FindData.dwReserved0 == IO_REPARSE_TAG_SYMLINK) {
                        if((FindData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) && (FindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) {
                            SkipDir = true;
                        }
                    }
                }
                
                if(!SkipDir)
                {
                     RecursiveIndex(FullBuffer);
                }
            }
            
        } while(FindNextFileW(FindHandle, &FindData));
        FindClose(FindHandle);
    }
}

DWORD WINAPI IndexerThreadProc(LPVOID Param)
{
    GlobalIndex.IsBuilding = true;
    
    if(!LoadIndexFromDisk())
    {
        if(GlobalIndex.Count == 0 && GlobalIndex.TotalIndexedCount == 0)
        {
            DWORD Drives = GetLogicalDrives();
            for(char Letter='A'; Letter<='Z'; Letter++)
            {
                if(Drives & 1)
                {
                    wchar_t DrivePath[10];
                    wsprintfW(DrivePath, L"%c:", Letter);
                    
                    UINT Type = GetDriveTypeW(DrivePath);
                    if(Type == DRIVE_FIXED)
                    {
                        wchar_t ScanPath[16];
                        wsprintfW(ScanPath, L"%c:\\", Letter);
                        RecursiveIndex(ScanPath);
                    }
                }
                Drives >>= 1;
            }
            if(GlobalIndex.Count > 0) SaveIndexChunkToDisk();
        }
    }
    
    GlobalIndex.IsReady = true;
    GlobalIndex.IsBuilding = false;
    return 0;
}

void
InitializeSearchIndex()
{
    u32 IndexMemorySize = Megabytes(16); 
    void *IndexMemory = VirtualAlloc(0, IndexMemorySize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
    InitializeArena(&GlobalIndex.Arena, IndexMemorySize, (u8*)IndexMemory);
    
    GlobalIndex.Capacity = MAX_INDEX_ENTRIES;
    GlobalIndex.Entries = PushArray(&GlobalIndex.Arena, GlobalIndex.Capacity, index_entry);
    GlobalIndex.Count = 0;
    
    CreateThread(0, 0, IndexerThreadProc, 0, 0, 0);
}

internal int CompareSearchScore(const void *A, const void *B) {
    file_info *FA = (file_info*)A;
    file_info *FB = (file_info*)B;
    return FB->SearchScore - FA->SearchScore; // Descending
}

internal directory_list
QueryIndex(memory_arena *Arena, wchar_t *Query)
{
    directory_list Result = {0};
    u32 MaxResults = 5000;
    Result.Files = (file_info *)PushArray(Arena, MaxResults, file_info);
    
    HANDLE File = CreateFileW(GlobalAppData_Index, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(File == INVALID_HANDLE_VALUE) return Result;
    
    HANDLE Map = CreateFileMappingA(File, 0, PAGE_READONLY, 0, 0, 0);
    if(Map)
    {
        u8 *Data = (u8 *)MapViewOfFile(Map, FILE_MAP_READ, 0, 0, 0);
        if(Data)
        {
            LARGE_INTEGER FileSize;
            GetFileSizeEx(File, &FileSize);
            u8 *End = Data + FileSize.QuadPart;
            u8 *Cursor = Data;
            
            while(Cursor < End)
            {
                if(Result.Count >= MaxResults) break;
                
                u32 PathLen = *(u32*)Cursor; Cursor += sizeof(u32);
                wchar_t *FullPath = (wchar_t *)Cursor; Cursor += PathLen * sizeof(wchar_t);
                u64 Size = *(u64*)Cursor; Cursor += sizeof(u64);
                FILETIME LWT = *(FILETIME*)Cursor; Cursor += sizeof(FILETIME);
                b32 IsDir = *(b32*)Cursor; Cursor += sizeof(b32);
                
                wchar_t Buffer[LONG_PATH];
                u32 CopyLen = PathLen;
                if(CopyLen >= LONG_PATH) CopyLen = LONG_PATH - 1;
                memcpy(Buffer, FullPath, CopyLen * sizeof(wchar_t));
                Buffer[CopyLen] = 0;
                
                b32 Match = false;
                i32 Score = 0;
                
                wchar_t *LastSlash = wcsrchr(Buffer, L'\\');
                wchar_t *FileName = LastSlash ? LastSlash + 1 : Buffer;
                
                if(_wcsicmp(FileName, Query) == 0) { Match = true; Score = 1000; }
                else if(ContainsW(FileName, Query)) { Match = true; Score = 500; }
                else if(ContainsW(Buffer, Query)) { Match = true; Score = 100; }
                
                if(Match)
                {
                     file_info *F = Result.Files + Result.Count;
                     F->FileName = PushStringW(Arena, FileName);
                     F->Size = Size;
                     F->LastWriteTime = LWT;
                     F->IsDirectory = IsDir;
                     F->SearchScore = Score;
                     
                     SYSTEMTIME ST; FileTimeToSystemTime(&LWT, &ST);
                     wsprintfA(F->DateString, "%02d/%02d/%04d %02d:%02d", ST.wDay, ST.wMonth, ST.wYear, ST.wHour, ST.wMinute);
                     
                     if(IsDir) {
                         wsprintfA(F->SizeString, "<DIR>"); 
                     } else {
                         double S = (double)Size;
                         if(S >= 1024.0*1024.0*1024.0) wsprintfA(F->SizeString, "%d.%d GB", (int)(S/(1024.0*1024.0*1024.0)), (int)(((S/(1024.0*1024.0*1024.0))-(int)(S/(1024.0*1024.0*1024.0)))*10));
                         else if(S >= 1024.0*1024.0) wsprintfA(F->SizeString, "%d.%d MB", (int)(S/(1024.0*1024.0)), (int)(((S/(1024.0*1024.0))-(int)(S/(1024.0*1024.0)))*10));
                         else wsprintfA(F->SizeString, "%d KB", (int)(S/1024.0));
                     }
                     Result.Count++;
                }
            }
            UnmapViewOfFile(Data);
        }
        CloseHandle(Map);
    }
    CloseHandle(File);
    
    if(Result.Count > 0) {
        qsort(Result.Files, Result.Count, sizeof(file_info), CompareSearchScore);
    }
    return Result;
}
