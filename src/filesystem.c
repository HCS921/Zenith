#ifndef FILESYSTEM_C
#define FILESYSTEM_C

#include "filesystem.h"

internal u32
StringLengthW(wchar_t *String)
{
    u32 Count = 0;
    while(*String++)
    {
        ++Count;
    }
    return Count;
}

internal wchar_t *
PushStringW(memory_arena *Arena, wchar_t *Source)
{
    u32 Length = StringLengthW(Source);
    u32 Size = (Length + 1) * sizeof(wchar_t);
    wchar_t *Dest = (wchar_t *)PushSize_(Arena, Size);
    for(u32 Index = 0; Index < Length; ++Index)
    {
        Dest[Index] = Source[Index];
    }
    Dest[Length] = 0;
    return Dest;
}

internal void
NormalizePathW(wchar_t *Dest, wchar_t *Source)
{
    int Len = lstrlenW(Source);
    if (Len >= MAX_PATH - 12) // Near limit, or already long
    {
        if (Source[0] == L'\\' && Source[1] == L'\\' && Source[2] == L'?' && Source[3] == L'\\')
        {
            lstrcpyW(Dest, Source);
        }
        else
        {
            // Prefix with \\?\ for long path support
            // Note: This requires the path to be absolute.
            wsprintfW(Dest, L"\\\\?\\%s", Source);
        }
    }
    else
    {
        lstrcpyW(Dest, Source);
    }
}

internal directory_list
ScanDirectory(memory_arena *Arena, wchar_t *DirectoryPath)
{
    directory_list Result = {0};
    
    // Scatch pad / max files assumption for now
    wchar_t SearchPath[LONG_PATH]; 
    wchar_t NormPath[LONG_PATH];
    NormalizePathW(NormPath, DirectoryPath);
    
    u32 PathLen = lstrlenW(NormPath);
    if (PathLen > LONG_PATH - 5) return Result;
    
    lstrcpyW(SearchPath, NormPath);
    if(PathLen > 0 && SearchPath[PathLen-1] != L'\\')
    {
        SearchPath[PathLen++] = L'\\';
    }
    SearchPath[PathLen++] = L'*';
    SearchPath[PathLen] = 0;
    
    WIN32_FIND_DATAW FindData;
    HANDLE FindHandle = FindFirstFileExW(
        SearchPath,
        FindExInfoBasic,
        &FindData,
        FindExSearchNameMatch,
        0,
        FIND_FIRST_EX_LARGE_FETCH
    );
    
    if(FindHandle != INVALID_HANDLE_VALUE)
    {
        u32 MaxFilesAprox = 10000;
        file_info *BaseFiles = PushArray(Arena, MaxFilesAprox, file_info);
        u32 Count = 0;
        
        do 
        {
            if(wcscmp(FindData.cFileName, L".") == 0 || wcscmp(FindData.cFileName, L"..") == 0) continue;
            
            if(Count >= MaxFilesAprox) break;
            
            file_info *File = BaseFiles + Count;
            File->Size = ((u64)FindData.nFileSizeHigh << 32) | FindData.nFileSizeLow;
            File->IsDirectory = (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            File->FileName = PushStringW(Arena, FindData.cFileName);
            File->LastWriteTime = FindData.ftLastWriteTime;
            
            // Format Strings Immediately (Perf Optimization for Render Loop)
            // Date
            SYSTEMTIME ST; 
            FileTimeToSystemTime(&File->LastWriteTime, &ST);
            wsprintfA(File->DateString, "%02d/%02d/%04d %02d:%02d", ST.wDay, ST.wMonth, ST.wYear, ST.wHour, ST.wMinute);
            
            // Size
            if(File->IsDirectory)
            {
                wchar_t FullPath[LONG_PATH];
                int P_Len = lstrlenW(DirectoryPath);
                if(DirectoryPath[P_Len-1] != L'\\') wsprintfW(FullPath, L"%s\\%s", DirectoryPath, File->FileName);
                else wsprintfW(FullPath, L"%s%s", DirectoryPath, File->FileName);
                
                File->Size = GetCachedSize(FullPath);
                
                if(File->Size != (u64)-1)
                {
                    f32 SizeMB = (f32)File->Size / (1024.0f*1024.0f);
                    if(SizeMB < 1.0f) {
                        int KB = (int)((f32)File->Size/1024.0f);
                        wsprintfA(File->SizeString, "%d KB", KB); 
                    } else {
                        int Whole = (int)SizeMB;
                        int Frac = (int)((SizeMB - Whole) * 10);
                        wsprintfA(File->SizeString, "%d.%d MB", Whole, Frac);
                    }
                }
                else
                {
                    File->SizeString[0] = '.'; File->SizeString[1] = '.'; File->SizeString[2] = '.'; File->SizeString[3] = 0;
                }
            }
            else
            {
                f32 SizeMB = (f32)File->Size / (1024.0f*1024.0f);
                if(SizeMB < 1.0f) 
                {
                    // Kb
                    f32 KB = (f32)File->Size/1024.0f;
                    int IntKB = (int)KB;
                    wsprintfA(File->SizeString, "%d KB", IntKB); 
                }
                else 
                {
                    // MB (wsprintfA doesn't support floating point well usually? It depends. 
                    // Standard wsprintf DOES NOT support %f. 
                    // We must use integer math or fallback to sprintf_s if available.
                    // Let's use integer math for "10.2 MB". 
                    // 10.2 = 102 / 10.
                    int Whole = (int)SizeMB;
                    int Frac = (int)((SizeMB - Whole) * 10);
                    wsprintfA(File->SizeString, "%d.%d MB", Whole, Frac);
                }
            }
            
            Count++;
            
        } while(FindNextFileW(FindHandle, &FindData));
        
        FindClose(FindHandle);
        
        Result.Count = Count;
        Result.Files = BaseFiles;
    }
    
    return Result;
}

internal u32
ReadFileChunk(wchar_t *FileName, void *Buffer, u32 BufferSize, b32 *Success)
{
    if(Success) *Success = false;
    
    // Use sharing flags to allow reading files even if they are open for write/delete elsewhere
    wchar_t NormPath[LONG_PATH];
    NormalizePathW(NormPath, FileName);
    HANDLE FileHandle = CreateFileW(NormPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, 0, 0);

    if(FileHandle == INVALID_HANDLE_VALUE) return 0;
    
    DWORD BytesRead = 0;
    if(ReadFile(FileHandle, Buffer, BufferSize, &BytesRead, 0))
    {
        if(Success) *Success = true;
    }
    
    CloseHandle(FileHandle);
    
    return (u32)BytesRead;
}

// --- Sorting ---

typedef int (*compare_func)(const void *, const void *);

internal int CompareNameAsc(const void *A, const void *B) { return lstrcmpiW(((file_info*)A)->FileName, ((file_info*)B)->FileName); }
internal int CompareNameDesc(const void *A, const void *B) { return lstrcmpiW(((file_info*)B)->FileName, ((file_info*)A)->FileName); }

internal int CompareSizeAsc(const void *A, const void *B) { return (((file_info*)A)->Size > ((file_info*)B)->Size) ? 1 : -1; }
internal int CompareSizeDesc(const void *A, const void *B) { return (((file_info*)A)->Size < ((file_info*)B)->Size) ? 1 : -1; }

internal int CompareDateAsc(const void *A, const void *B) { return CompareFileTime(&((file_info*)A)->LastWriteTime, &((file_info*)B)->LastWriteTime); }
internal int CompareDateDesc(const void *A, const void *B) { return CompareFileTime(&((file_info*)B)->LastWriteTime, &((file_info*)A)->LastWriteTime); }

internal void
SortFiles(directory_list *List, int Mode, b32 Ascending)
{
    if(List->Count < 2) return;
    
    compare_func Func = 0;
    if(Mode == 0) Func = Ascending ? CompareNameAsc : CompareNameDesc;
    if(Mode == 1) Func = Ascending ? CompareDateAsc : CompareDateDesc; // Note: Mode 1=Date per UI order usually
    if(Mode == 2) Func = Ascending ? CompareSizeAsc : CompareSizeDesc;
    
    if(Func) qsort(List->Files, List->Count, sizeof(file_info), Func);
    if(Func) qsort(List->Files, List->Count, sizeof(file_info), Func);
}

// --- Folder Size ---

internal u64
GetFolderSize(wchar_t *Directory, int Depth)
{
    if(Depth > 256) return 0; // Prevent Stack Overflow
    if(!Directory || !Directory[0]) return 0;
    
    u64 TotalSize = 0;
    
    wchar_t *SearchPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, LONG_PATH * sizeof(wchar_t));
    wchar_t *NormPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, LONG_PATH * sizeof(wchar_t));
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
            if(wcscmp(FindData.cFileName, L".") == 0 || wcscmp(FindData.cFileName, L"..") == 0) continue;
            
            if((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                TotalSize += ((u64)FindData.nFileSizeHigh << 32) | FindData.nFileSizeLow;
            }
            else // Directory
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
                     wchar_t *SubDir = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, LONG_PATH * sizeof(wchar_t));
                     lstrcpyW(SubDir, Directory);
                     int DirLen = lstrlenW(SubDir);
                     if(DirLen > 0 && SubDir[DirLen-1] != L'\\') SubDir[DirLen++] = L'\\';
                     lstrcpyW(SubDir + DirLen, FindData.cFileName);
                     TotalSize += GetFolderSize(SubDir, Depth + 1);
                     HeapFree(GetProcessHeap(), 0, SubDir);
                }
            }
            
        } while(FindNextFileW(FindHandle, &FindData));
        FindClose(FindHandle);
    }
    
    HeapFree(GetProcessHeap(), 0, SearchPath);
    HeapFree(GetProcessHeap(), 0, NormPath);
    
    return TotalSize;
}

// --- Async Jobs ---

extern work_queue GlobalWorkQueue;

internal void
CalculateFolderSizeJob(void *Data)
{
    wchar_t *Path = (wchar_t *)Data;
    extern void LogMessage(const char *Format, ...);
    
    LogMessage("Job Start: Calc Size DEEP SCAN for '%S'", Path);
    u64 Size = GetFolderSize(Path, 0);
    
    LogMessage("Job Done: Size %llu bytes", Size);

    // Update Cache
    SetCachedSize(Path, Size);
    
    // Save occasionally? Or Main Thread saves?
    // Let's rely on Main Thread or auto-save on change.
    
    HeapFree(GetProcessHeap(), 0, Path);
}

// --- Storage Analysis ---

internal storage_node *
AnalyzeStorage(memory_arena *Arena, wchar_t *Path, int Depth)
{
    if(Depth > 10) return 0; // Hard Limit
    
    // Safety Check
    if((Arena->Used + sizeof(storage_node) + 256) > Arena->Size) return 0;

    if(!Path || !Path[0]) 
    {
        // --- HOME VIEW (This PC) ---
        storage_node *Root = PushStruct(Arena, storage_node);
        Root->Name = L"This PC";
        Root->FullPath = L"";
        Root->IsDirectory = true;
        Root->IsExpanded = true;
        Root->IndentLevel = 0;
        Root->Size = 0;
        
        DWORD Drives = GetLogicalDrives();
        storage_node *LastChild = 0;
        for(char Letter='A'; Letter<='Z'; Letter++)
        {
            if(Drives & 1)
            {
                wchar_t DrivePath[4] = { (wchar_t)Letter, L':', L'\\', 0 };
                // Shallow Analyze each drive
                storage_node *DriveNode = AnalyzeStorage(Arena, DrivePath, Depth + 1);
                if(DriveNode)
                {
                    DriveNode->Parent = Root;
                    if(LastChild) LastChild->NextSibling = DriveNode;
                    else Root->FirstChild = DriveNode;
                    LastChild = DriveNode;
                    Root->ChildCount++;
                    Root->Size += DriveNode->Size;
                }
            }
            Drives >>= 1;
        }
        return Root;
    }

    extern void LogMessage(const char *Format, ...);
    LogMessage("AnalyzeStorage: '%S'", Path);

    storage_node *Node = PushStruct(Arena, storage_node);
    Node->FullPath = PushStringW(Arena, Path);
    
    // Explicitly zero remaining fields due to arena reuse
    Node->Parent = 0;
    Node->FirstChild = 0;
    Node->NextSibling = 0;
    Node->ChildCount = 0;
    Node->IsPending = false;
    Node->IsExpanded = false;
    Node->ChildrenPopulated = false;
    Node->IndentLevel = Depth;
    Node->Size = 0;

    // Extract Name
    wchar_t *LastSlash = wcsrchr(Path, L'\\');
    if(LastSlash) {
        if(*(LastSlash + 1) == 0) { // e.g. "C:\"
             Node->Name = PushStringW(Arena, Path); // Keep C:\ as name for drives
        } else {
             Node->Name = PushStringW(Arena, LastSlash + 1);
        }
    } else {
        Node->Name = PushStringW(Arena, Path);
    }
    
    Node->IsDirectory = true;
    
    // Check Cache for THIS node (The total size of this folder)
    u64 Cached = GetCachedSize(Path);
    if(Cached != (u64)-1)
    {
        Node->Size = Cached;
    }
    else
    {
        wchar_t *JobPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lstrlenW(Path)+1)*sizeof(wchar_t));
        lstrcpyW(JobPath, Path);
        AddWork(&GlobalWorkQueue, CalculateFolderSizeJob, JobPath);
        
        Node->Size = (u64)-1; 
    }

    Node->FirstChild = 0;
    Node->ChildCount = 0;

    // Shallow Scan for Children (Visible Tree)
    // Only scan immediate children if we are the Root or explicitly drilling down?
    // RenderStorageView wants to show Children bars.
    // So we MUST populate Children.
    
    wchar_t SearchPath[LONG_PATH];
    wchar_t NormPath[LONG_PATH];
    NormalizePathW(NormPath, Path);
    int P_Len = lstrlenW(NormPath);
    if(P_Len >= LONG_PATH - 5) return Node; // Path too long

    if(P_Len > 0 && NormPath[P_Len-1] == L'\\') wsprintfW(SearchPath, L"%s*", NormPath);
    else wsprintfW(SearchPath, L"%s\\*", NormPath);
    
    WIN32_FIND_DATAW FindData;
    HANDLE FindHandle = FindFirstFileW(SearchPath, &FindData);
    
    if(FindHandle != INVALID_HANDLE_VALUE)
    {
        storage_node *LastChild = 0;
        
        do 
        {
            if(wcscmp(FindData.cFileName, L".") == 0 || wcscmp(FindData.cFileName, L"..") == 0) continue;
            
            storage_node *Child = 0;
            u64 ChildSize = ((u64)FindData.nFileSizeHigh << 32) | FindData.nFileSizeLow;
            
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
                     // Directory
                     // Do NOT recurse AnalyzeStorage deeply.
                     // Just create a Child Node and check CACHE.
                     
                     if((Arena->Used + sizeof(storage_node) + 128) > Arena->Size) { continue; }

                     Child = PushStruct(Arena, storage_node);
                     Child->Name = PushStringW(Arena, FindData.cFileName);
                     
                     wchar_t SubPath[LONG_PATH];
                     if(lstrlenW(Path) + lstrlenW(FindData.cFileName) + 2 >= LONG_PATH) continue;
                     wsprintfW(SubPath, L"%s\\%s", Path, FindData.cFileName);
                     Child->FullPath = PushStringW(Arena, SubPath); 
                     
                     Child->IsDirectory = true;
                     Child->ChildCount = 0;
                     Child->FirstChild = 0;
                     Child->NextSibling = 0;
                     Child->IsExpanded = false;
                     Child->ChildrenPopulated = false;
                     Child->IndentLevel = Depth + 1;
                     
                     u64 ChildCached = GetCachedSize(SubPath);
                     if(ChildCached != (u64)-1)
                     {
                         Child->Size = ChildCached;
                     }
                     else
                     {
                         Child->Size = (u64)-1;
                         // Queue Job
                         wchar_t *JobPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lstrlenW(SubPath)+1)*sizeof(wchar_t));
                         lstrcpyW(JobPath, SubPath);
                         AddWork(&GlobalWorkQueue, CalculateFolderSizeJob, JobPath);
                     }
                }
            }
            else
            {
                // File Node
                if((Arena->Used + sizeof(storage_node) + 128) > Arena->Size) { continue; } 
                Child = PushStruct(Arena, storage_node);
                Child->Name = PushStringW(Arena, FindData.cFileName);
                Child->FullPath = 0; 
                Child->Size = ChildSize;
                Child->IsDirectory = false;
                Child->ChildCount = 0;
                Child->FirstChild = 0;
                Child->NextSibling = 0;
                Child->IsExpanded = false;
                Child->ChildrenPopulated = true; // Leaf
                Child->IndentLevel = Depth + 1;
            }

            if(Child)
            {
                Node->ChildCount++;
                if(LastChild) LastChild->NextSibling = Child;
                else Node->FirstChild = Child;
                LastChild = Child;
            }
            
        } while(FindNextFileW(FindHandle, &FindData));
        FindClose(FindHandle);
    }
    
    // If Size came from Cache, we are good.
    // Otherwise, we sum up what we found during the shallow scan natively without bypassing un-scanned markers.
    if(Node->Size == (u64)-1)
    {
        u64 FallbackSum = 0;
        storage_node *Child = Node->FirstChild;
        while(Child)
        {
            if (Child->Size != (u64)-1 && Child->Size != (u64)-2) {
                FallbackSum += Child->Size;
            }
            Child = Child->NextSibling;
        }
        if (FallbackSum > 0) Node->Size = FallbackSum;
        // else remains (u64)-1 natively enforcing the "..." pending state explicitly via render loop
    }
    
    Node->ChildrenPopulated = true;
    return Node;
}

#endif // FILESYSTEM_C
