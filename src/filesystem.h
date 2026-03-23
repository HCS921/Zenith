#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "defs.h"
#include "memory.h"
#include <windows.h>

typedef struct file_info
{
    u64 Size;
    b32 IsDirectory;
    wchar_t *FileName;
    FILETIME LastWriteTime;
    char DateString[32];
    char SizeString[32];
    i32 SearchScore;
} file_info;

typedef struct directory_list
{
    u32 Count;
    file_info *Files;
} directory_list;

// Scans a directory and pushes results on the arena
internal directory_list ScanDirectory(memory_arena *Arena, wchar_t *DirectoryPath);
internal directory_list ScanDirectory(memory_arena *Arena, wchar_t *Directory);
internal u32 ReadFileChunk(wchar_t *FileName, void *Buffer, u32 BufferSize, b32 *Success);

// Helper to get folder size
u64 GetFolderSize(wchar_t *Path, int Depth);

// Helper to sort files
void SortFiles(directory_list *List, int Mode, b32 Asc);

// --- Storage Cache Helpers ---
u64 GetCachedSize(wchar_t *Path);
void SetCachedSize(wchar_t *Path, u64 Size);
b32 IsCacheModified();
void SaveCache();
void InitCache();

// --- Storage Tree ---
typedef struct storage_node {
    wchar_t *Name;
    wchar_t *FullPath;
    u64 Size;
    struct storage_node *Parent;
    struct storage_node *FirstChild;
    struct storage_node *NextSibling;
    u32 ChildCount;
    b32 IsDirectory;
    b32 IsPending;
    b32 IsExpanded;
    b32 ChildrenPopulated;
    int IndentLevel;
} storage_node;

// --- Search Index ---
typedef struct {
    wchar_t *FullPath;
    u64 Size;
    FILETIME LastWriteTime;
    b32 IsDirectory;
} index_entry;

#define MAX_INDEX_ENTRIES 50000 

typedef struct {
    memory_arena Arena;
    index_entry *Entries;
    volatile u32 Count;
    volatile u64 TotalIndexedCount; 
    u32 Capacity;
    
    volatile b32 IsReady;
    volatile b32 IsBuilding;
    volatile b32 StopRequested;
} search_index;

internal storage_node *AnalyzeStorage(memory_arena *Arena, wchar_t *Path, int Depth);
internal directory_list QueryIndex(memory_arena* Arena, wchar_t* Query);

#endif // FILESYSTEM_H
