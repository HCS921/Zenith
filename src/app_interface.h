#ifndef APP_INTERFACE_H
#define APP_INTERFACE_H

#include <windows.h>
#include "defs.h"
#include "memory.h"
#include "filesystem.h"
#include "renderer.h"
#include "font.h"
#include "inspector.h"
#include "multithreading.h"

// --- Icons ---
typedef enum {
    ICON_FOLDER,
    ICON_DRIVE_INTERNAL,
    ICON_DRIVE_EXTERNAL,
    ICON_FILE_PYTHON,
    ICON_FILE_TEXT,
    ICON_FILE_EXE,
    ICON_FILE_DLL,
    ICON_FILE_CODE,
    ICON_FILE_IMAGE,
    ICON_FILE_GENERIC,
    ICON_APP_FACE,
    ICON_SEARCH
} icon_type;

void Render8BitIcon(icon_type Type, f32 X, f32 Y, f32 Scale);
icon_type DetermineIconType(wchar_t *Name, b32 IsDirectory);

// --- UI Components ---
void RenderSidebar(renderer_state *Renderer, font_atlas *Font, f32 X, f32 Y, f32 W, f32 H, 
                   int MouseX, int MouseY, f32 *SidebarHoverColor, f32 *TextColor, f32 *MutedColor, f32 *HoverT);

// --- Views ---
void RenderStorageView(f32 X, f32 Y, f32 W, f32 H, storage_node *Tree, f32 ScrollY, int MouseX, int MouseY, b32 Clicked);
void RenderHomeView(f32 X, f32 Y, f32 W, f32 H, b32 Clicked);

// --- Search ---
void InitializeSearchIndex();
directory_list QueryIndex(memory_arena *Arena, wchar_t *Query);

// --- Storage Cache ---
u64 GetCachedSize(wchar_t *Path);
void SetCachedSize(wchar_t *Path, u64 Size);
b32 IsCacheModified();
void SaveCache();
void InitCache();

// --- Main App Logic ---
void SortFiles(directory_list *Dir, int Mode, b32 Ascending);
u64 GetFolderSize(wchar_t *Path, b32 Recursive);
void ChangeDirectory(wchar_t *NewPath, b32 AddToHistory);
void LogMessage(const char *Format, ...);

// --- Global App State (Shared) ---
extern f32 GlobalScale;
extern int GlobalViewMode;
extern storage_node *GlobalStorageTree;
extern volatile b32 GlobalIsAnalyzing;
extern wchar_t GlobalCurrentPath[MAX_PATH];

#endif
