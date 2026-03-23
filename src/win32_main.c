#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include "app_interface.h"

// Define missing GUID for BindToHandler
const GUID BHID_PreviewHandler_Custom = { 0xb824b49d, 0x22ce, 0x43f7, { 0x85, 0xa1, 0xbd, 0x71, 0x3d, 0x69, 0x7c, 0xad } };

// Note: search_engine.c functions will be visible if included at bottom and prototypes match.

// Global running state
global_variable b32 GlobalRunning;
// renderer_state GlobalRenderer; // Defined later below (Line 77 in viewed file)
global_variable memory_arena GlobalPartition;
global_variable memory_arena GlobalPreviewArena; // Dedicated for preview list
global_variable memory_arena GlobalStorageArena; // Dedicated for storage tree (folder sizes)
global_variable directory_list GlobalDir;
global_variable directory_list GlobalPreviewDir;
global_variable wchar_t GlobalCurrentPath[MAX_PATH] = {0};
global_variable char GlobalPreviewBuffer[65536]; // 64KB Preview Buffer
// Queue
work_queue GlobalWorkQueue;
global_variable b32 GlobalPreviewLoaded = false;
global_variable ID3D11ShaderResourceView *GlobalPreviewTexture = 0; // WIC Texture
global_variable IPreviewHandler *GlobalPreviewHandler = 0; // COM Native Previewer
global_variable HWND GlobalMainWindow = 0; // Reference to window for handler
global_variable HICON GlobalAppIcon = 0; // Global reference to prevent GC/Release issues

global_variable f32 GlobalOpenBtnHoverT = 0.0f;     // "Open" button animation timer
global_variable f32 GlobalListSelectionAlpha = 0.0f; // Selection highlight opacity factor
global_variable f32 GlobalListHoverT[1024] = {0};   // Row-level hover timers for smooth highlight fades
global_variable f32 GlobalScrollY = 0.0f;           // Current smoothed scroll position
global_variable f32 GlobalTargetScrollY = 0.0f;
global_variable f32 GlobalPreviewScrollY = 0.0f;
global_variable int GlobalSelectedIndex = -1;
global_variable FILETIME GlobalCurrentPathLastWriteTime = {0};

global_variable f32 GlobalListTop = 40.0f;
global_variable int GlobalMouseX = 0;
global_variable int GlobalMouseY = 0;
global_variable b32 GlobalContextMenuActive = false;
global_variable int GlobalContextMenuX = 0;
global_variable int GlobalContextMenuY = 0;
global_variable int GlobalContextMenuIndex = -1;
global_variable char GlobalMenuSearch[64]; 
global_variable f32 GlobalMenuAlpha = 0.0f; // For animation
global_variable b32 GlobalIsDragging = false;
global_variable int GlobalDragStartX = 0;
global_variable int GlobalDragStartY = 0;
global_variable int GlobalDragEndX = 0;
global_variable int GlobalDragEndY = 0;

global_variable wchar_t GlobalSelectedDrive[16]; // Store selected drive for context menu

// Shared Globals (Linkage: External)
int GlobalViewMode = 0; // 0=List, 1=StorageTree
storage_node *GlobalStorageTree = 0; // Tree for Visualization
volatile b32 GlobalIsAnalyzing = false;
wchar_t GlobalAnalysisPath[MAX_PATH];
wchar_t GlobalAppData_Cache[MAX_PATH];
wchar_t GlobalAppData_Index[MAX_PATH];

DWORD WINAPI StorageAnalysisThread(LPVOID Param)
{
    // RESET ARENA before starting a new analysis to prevent Cumulative Overflow Assertion Crash
    GlobalStorageArena.Used = 0;
    
    // Debug Log Removed for Production
    GlobalStorageTree = AnalyzeStorage(&GlobalStorageArena, GlobalAnalysisPath, 0);
    if(GlobalStorageTree) {
        GlobalStorageTree->IsExpanded = true; 
    }
    
    // Done
    GlobalIsAnalyzing = false;
    return 0;
}
global_variable b32 GlobalSearchAllDrives = false;

global_variable b32 GlobalSearchFocused = false;
global_variable char GlobalSearchBuffer[MAX_PATH] = {0};
global_variable f32 GlobalSearchHoverT = 0.0f;

global_variable b32 GlobalSearchHot = false;
global_variable int GlobalSortMode = 0; // 0=Name, 1=Date, 2=Size
global_variable b32 GlobalSortAsc = true;

// global_variable storage_node *GlobalStorageTree = 0; // Moved UP
global_variable f32 GlobalMenuScale = 0.0f; // Scale for animation (0->1)
global_variable int GlobalInspectorTab = 0; // 0=Preview, 1=Storage
global_variable f32 GlobalNavTransitionT = 0.0f; // 0.0 -> 1.0 (Animation Factor)
global_variable b32 GlobalNavDirection = 0;    // 0 = Forward, 1 = Back
global_variable b32 GlobalStorageVisualizeMode = false; // Toggle for storage visualization
// Hover defined above
global_variable f32 GlobalSidebarHoverT[32] = {0}; // Sidebar item hover animation (0-5: Pinned, 6+: Drives)
global_variable f32 GlobalTime = 0.0f; // Global time in seconds
extern search_index GlobalIndex;
extern DWORD WINAPI IndexerThreadProc(LPVOID Param);

// Menu Items
typedef struct { char *Name; int Id; } menu_item;
// IDs: 0=Open, 1=CopyPath, 2=Properties, 3=CalculateSize
menu_item ContextMenuItems[] = {
    {"Open", 0},
    {"Copy Path", 1},
    {"Properties", 2},
    {"Analyze Storage", 3},  // NEW ACTION
};


renderer_state GlobalRenderer = {0};
// GlobalSearchBuffer defined above

// History
typedef struct history_node {
    wchar_t Path[MAX_PATH];
    struct history_node *Next;
} history_node;

global_variable history_node *GlobalHistory = 0;

// Font
font_atlas Font = {0}; // Moved to global and renamed to avoid conflict with local variable in WinMain
global_variable f32 GlobalHoverAlpha[100]; // Simple pool for UI fades? Or just immediate mode lerp? 
// Immediate mode usually uses a "State" map. 
// For simplicity: We will just lerp generic "Hot" values per frame if we track ID? 
// Or just basic "Slide" values.
global_variable f32 GlobalSidebarSlide = 0.0f; // For entry animation


// Helper for Search
internal b32
CaseInsensitiveContains(char *Haystack, char *Needle)
{
    if(!Needle[0]) return true;
    while(*Haystack)
    {
        char *H = Haystack;
        char *N = Needle;
        while(*H && *N)
        {
            char h = *H; if(h >= 'A' && h <= 'Z') h += 32;
            char n = *N; if(n >= 'A' && n <= 'Z') n += 32;
            if(h != n) break;
            H++; N++;
        }
        if(!*N) return true;
        Haystack++;
    }
    return false;
}

// Helper: Format File Time
internal void
FormatDate(FILETIME FT, char *Buf, int Size)
{
    SYSTEMTIME ST;
    FileTimeToSystemTime(&FT, &ST);
    sprintf_s(Buf, Size, "%02d/%02d/%04d %02d:%02d", ST.wDay, ST.wMonth, ST.wYear, ST.wHour, ST.wMinute);
}

// Globals
f32 GlobalScale = 1.0f;
f32 GlobalRowHeight = 32.0f; 
b32 GlobalLeftClickOneShot = false; 

ID3D11ShaderResourceView *GlobalDriveTexture = 0;
f32 GlobalHomeAnimT = 0.0f;

// Logging Helper
#include <stdarg.h>
void LogMessage(const char *Format, ...)
{
    // Redacted for production to prevent local directory telemetry or slop file generation
}

// Forward declarations
LRESULT CALLBACK Win32WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

// Quick Helpers
internal void PushHistory(wchar_t *Path);
internal void ChangeDirectory(wchar_t *NewPath, b32 AddHistory);
internal void GoBack();

// Helper to change directory safely
internal void
PushHistory(wchar_t *Path)
{
    history_node *Node = (history_node *)HeapAlloc(GetProcessHeap(), 0, sizeof(history_node));
    if(Node)
    {
        wsprintfW(Node->Path, L"%s", Path);
        Node->Next = GlobalHistory;
        GlobalHistory = Node;
    }
}

internal void
ChangeDirectory(wchar_t *NewPath, b32 AddHistory)
{
    if(AddHistory && GlobalCurrentPath[0])
    {
        PushHistory(GlobalCurrentPath);
    }
    GlobalPartition.Used = 0; 
    GlobalDir = ScanDirectory(&GlobalPartition, NewPath);
    
    // Auto-Sort
    SortFiles(&GlobalDir, GlobalSortMode, GlobalSortAsc);
    
    u32 Len = lstrlenW(NewPath);
    if(Len >= MAX_PATH) Len = MAX_PATH-1;
    for(u32 i=0; i<Len; ++i) GlobalCurrentPath[i] = NewPath[i];
    GlobalCurrentPath[Len] = 0;
    GlobalScrollY = 0;
    GlobalTargetScrollY = 0;
    GlobalPreviewScrollY = 0;
    GlobalSelectedIndex = -1;
    GlobalPreviewLoaded = false;
    
    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesExW(NewPath, GetFileExInfoStandard, &Data)) {
        GlobalCurrentPathLastWriteTime = Data.ftLastWriteTime;
    } else {
        GlobalCurrentPathLastWriteTime.dwLowDateTime = 0;
        GlobalCurrentPathLastWriteTime.dwHighDateTime = 0;
    }
    
    GlobalNavTransitionT = 1.0f; // Trigger Animation
    GlobalNavDirection = 0;      // Forward
    
    // NOTE: We don't Wipe StorageTree here if we are about to enter Storage Mode.
    // If we are in List Mode, we might want a quick shadow scan for folder sizes.
    if(GlobalViewMode == 0)
    {
        extern memory_arena GlobalStorageArena;
        GlobalStorageArena.Used = 0; 
        GlobalStorageTree = AnalyzeStorage(&GlobalStorageArena, GlobalCurrentPath, 0); // Shallow
    }
}

internal void
GoBack()
{
    // If in Storage Visualization, Back means "Close Visualization"
    if(GlobalViewMode != 0)
    {
        GlobalViewMode = 0;
        GlobalInspectorTab = 0; // Reset Inspector
        return;
    }

    if(GlobalCurrentPath[0] == 0) return; // Already at home

    wchar_t UpPath[MAX_PATH];
    lstrcpyW(UpPath, GlobalCurrentPath);
    int Len = lstrlenW(UpPath);
    
    // Strip trailing slash if not root drive
    if(Len > 3 && UpPath[Len-1] == L'\\') 
    {
        UpPath[Len-1] = 0;
    }
    
    wchar_t *LastSlash = wcsrchr(UpPath, L'\\');
    if(LastSlash) 
    {
        if(LastSlash == UpPath + 2 && UpPath[1] == L':') 
        {
            // It's like "C:\", so parent is HOME
            ChangeDirectory(L"", true);
        }
        else 
        {
            *(LastSlash) = 0; // Cut off the last part
            // If it becomes "C:", we need the slash back
            if(lstrlenW(UpPath) == 2 && UpPath[1] == L':') 
            {
                UpPath[2] = L'\\';
                UpPath[3] = 0;
            }
            ChangeDirectory(UpPath, true);
        }
    } 
    else 
    {
        ChangeDirectory(L"", true);
    }
    
    GlobalNavTransitionT = 1.0f;
    GlobalNavDirection = 1;
}

// Helper to safely restart indexer
DWORD WINAPI RestartIndexerThread(LPVOID Param)
{
    // 1. Request stop
    GlobalIndex.StopRequested = true;
    
    // 2. Wait for it to actually stop
    while(GlobalIndex.IsBuilding)
    {
        Sleep(10);
    }
    
    // 3. Reset State
    GlobalIndex.StopRequested = false;
    GlobalIndex.Count = 0;
    GlobalIndex.IsReady = false;
    GlobalIndex.IsBuilding = true;
    
    // Reset Arena (Safe now that no one is writing)
    GlobalIndex.Arena.Used = 0;
    
    // 4. Start New Indexer
    // We can just call IndexerThreadProc directly since WE are already in a thread!
    IndexerThreadProc(0);
    
    return 0;
}

// Forward declarations for Internal Functions (Unity Build)
internal void AddWork(work_queue *Queue, work_queue_callback Callback, void *Data);
internal void CalculateFolderSizeJob(void *Data);

int WINAPI
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    // 0. Enable High-DPI Support (Fixes Blurry Text)
    SetProcessDPIAware(); 
    
    // CRITICAL for WIC and Shell API
    CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Resolve Universal Sandbox AppData paths securely to bypass Program Files / Root protections natively
    wchar_t AppPath[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPathW(0, CSIDL_LOCAL_APPDATA, 0, 0, AppPath))) {
        wchar_t ZenithDir[MAX_PATH];
        wsprintfW(ZenithDir, L"%s\\Zenith", AppPath);
        CreateDirectoryW(ZenithDir, 0);
        wsprintfW(GlobalAppData_Cache, L"%s\\explorer_db.bin", ZenithDir);
        wsprintfW(GlobalAppData_Index, L"%s\\search_index.dat", ZenithDir);
    } else {
        lstrcpyW(GlobalAppData_Cache, L"explorer_db.bin");
        lstrcpyW(GlobalAppData_Index, L"search_index.dat");
    }
    
    // 0. Load Icon from Resource (ID 1)
    // This is the Bulletproof way. We load the embedded folder_icon.ico
    HICON AppIcon = LoadIcon(Instance, MAKEINTRESOURCE(1));

    WNDCLASSEXW WindowClass = {0};
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    WindowClass.lpfnWndProc = Win32WindowProc;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = L"ZenithWindowClass";
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    
    // Class Icon
    WindowClass.hIcon = AppIcon;
    WindowClass.hIconSm = AppIcon;

    if(RegisterClassExW(&WindowClass))
    {
        HWND Window = CreateWindowExW(
            0,
            WindowClass.lpszClassName,
            L"Zenith",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
            0, 0, Instance, 0);

        if(Window)
        {
            GlobalMainWindow = Window; // Store HWND globally
            // Aggressive Icon Enforcement
            SendMessage(Window, WM_SETICON, ICON_BIG, (LPARAM)AppIcon);
            SendMessage(Window, WM_SETICON, ICON_SMALL, (LPARAM)AppIcon);

            // Init Global Renderer
            if(!InitD3D11(Window, &GlobalRenderer))
            {
                // Fatal Error
                return 1;
            }

            // Init Memory (DRAM limits expanded for Billion-File Zenith stability)
            void *BaseMemory = VirtualAlloc(0, 1024*1024*256, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            InitializeArena(&GlobalPartition, 1024*1024*256, BaseMemory);
            
            // Preview Arena (32MB handles massive file metadata natively)
            void *PreviewMemory = VirtualAlloc(0, 1024*1024*32, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            InitializeArena(&GlobalPreviewArena, 1024*1024*32, PreviewMemory);
            
            // Storage Arena (512MB for ultra-deep OS directory trees)
            void *StorageMemory = VirtualAlloc(0, 1024*1024*512, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
            InitializeArena(&GlobalStorageArena, 1024*1024*512, StorageMemory);
            
            // Set App Icon
            void SetAppIcon(HWND Window);
            SetAppIcon(Window);

            // Init Threads
            InitializeWorkQueue(&GlobalWorkQueue, 8);

            // Initial Scan - START AT HOME (Empty Path)
            // Was manually injected here but causing a crash:

            // Init Search Indexer
            InitializeSearchIndex();
            // Init Storage Cache
            InitCache();

            // Init Font
            HDC ScreenDC = GetDC(0);
            int DpiY = GetDeviceCaps(ScreenDC, LOGPIXELSY);
            ReleaseDC(0, ScreenDC);
            GlobalScale = (f32)DpiY / 96.0f;
            if(GlobalScale < 1.0f) GlobalScale = 1.0f;
            
            int FontSize = (int)(18.0f * GlobalScale);
            GlobalRowHeight = 32.0f * GlobalScale;
            
            // USE GLOBAL FONT
            InitFontAtlas(&GlobalRenderer, &Font, "Segoe UI", FontSize);

            ChangeDirectory(L"", false);

            GlobalRunning = true;

            LARGE_INTEGER PerfFreqRel, StartPerf;
            QueryPerformanceFrequency(&PerfFreqRel);
            f64 PerfFreq = (f64)PerfFreqRel.QuadPart;
            QueryPerformanceCounter(&StartPerf);

            static int RefreshTicker = 0;

            while(GlobalRunning)
            {
                LARGE_INTEGER CurrentPerf;
                QueryPerformanceCounter(&CurrentPerf);
                GlobalTime = (f32)((f64)(CurrentPerf.QuadPart - StartPerf.QuadPart) / PerfFreq);

                MSG Message;
                while(PeekMessageW(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageW(&Message);
                }

                // Auto-Refresh Storage View / Folder Sizes (Async Results)
                RefreshTicker++;
                int RefreshRate = (GlobalViewMode == 1) ? 30 : 120; // 0.5s vs 2.0s
                if(RefreshTicker > RefreshRate)
                {
                    // 0. Live Update check for Current Folder!
                    if(GlobalViewMode == 0 && GlobalCurrentPath[0])
                    {
                        WIN32_FILE_ATTRIBUTE_DATA Data;
                        if(GetFileAttributesExW(GlobalCurrentPath, GetFileExInfoStandard, &Data))
                        {
                            if(CompareFileTime(&Data.ftLastWriteTime, &GlobalCurrentPathLastWriteTime) != 0)
                            {
                                 GlobalCurrentPathLastWriteTime = Data.ftLastWriteTime;
                                 ChangeDirectory(GlobalCurrentPath, false); // Auto refresh list view
                            }
                        }
                    }
                    
                    // 1. Live Folder Sizes in Main List
                    if(GlobalViewMode == 0)
                    {
                        for(u32 i=0; i<GlobalDir.Count; ++i)
                        {
                            file_info *File = GlobalDir.Files + i;
                            if(File->IsDirectory && File->Size == (u64)-1)
                            {
                                wchar_t FullPath[MAX_PATH];
                                int P_Len = lstrlenW(GlobalCurrentPath);
                                if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(FullPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                                else wsprintfW(FullPath, L"%s%s", GlobalCurrentPath, File->FileName);
                                
                                u64 Cached = GetCachedSize(FullPath);
                                if(Cached != (u64)-1)
                                {
                                    File->Size = Cached;
                                }
                                else
                                {
                                    if(GlobalWorkQueue.EntryCount < 200)
                                    {
                                        wchar_t *JobPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lstrlenW(FullPath)+1)*sizeof(wchar_t));
                                        if(JobPath) {
                                            lstrcpyW(JobPath, FullPath);
                                            AddWork(&GlobalWorkQueue, CalculateFolderSizeJob, JobPath);
                                            File->Size = (u64)-2; 
                                        }
                                    }
                                }
                            }
                            if(File->IsDirectory)
                            {
                                if(File->Size == (u64)-1 || File->Size == (u64)-2) {
                                    File->SizeString[0] = '.'; File->SizeString[1] = '.'; File->SizeString[2] = '.'; File->SizeString[3] = 0;
                                } else {
                                    double S = (double)File->Size;
                                    if(S >= 1024.0*1024.0*1024.0) {
                                        f32 SizeGB = (f32)(S / (1024.0*1024.0*1024.0));
                                        int Whole = (int)SizeGB;
                                        int Frac = (int)((SizeGB - Whole) * 10);
                                        wsprintfA(File->SizeString, "%d.%d GB", Whole, Frac);
                                    } else if(S >= 1024.0*1024.0) {
                                        f32 SizeMB = (f32)(S / (1024.0*1024.0));
                                        int Whole = (int)SizeMB;
                                        int Frac = (int)((SizeMB - Whole) * 10);
                                        wsprintfA(File->SizeString, "%d.%d MB", Whole, Frac);
                                    } else {
                                        wsprintfA(File->SizeString, "%d KB", (int)(S/1024.0));
                                    }
                                }
                            }
                        }
                    }
                    
                    // 2. Storage Tree Refresh (ONLY if not already doing it and not too frequent)
                    if(IsCacheModified() && !GlobalIsAnalyzing)
                    {
                         // Auto-refresh is risky if tree is huge. 
                         // For now, let's only refresh if we are already in storage view and idle.
                         /*
                         GlobalStorageArena.Used = 0;
                         GlobalStorageTree = AnalyzeStorage(&GlobalStorageArena, GlobalCurrentPath, 0);
                         */
                    }
                    RefreshTicker = 0;
                }
                
                // View Animation
                static f32 ViewAnim = 0.0f;
                f32 TargetAnim = (GlobalViewMode == 1) ? 1.0f : 0.0f;
                ViewAnim += (TargetAnim - ViewAnim) * 0.2f; // Fast Smooth
                
                // Home Opening Animation
                f32 TargetHomeAnim = (GlobalCurrentPath[0] == 0) ? 1.0f : 0.0f;
                GlobalHomeAnimT += (TargetHomeAnim - GlobalHomeAnimT) * 0.15f;

                // Update Search Bar Hover/Focus Animation
                RECT ClientRect_Anim;
                GetClientRect(Window, &ClientRect_Anim);
                f32 Width_Anim = (f32)(ClientRect_Anim.right - ClientRect_Anim.left);
                f32 Height_Anim = (f32)(ClientRect_Anim.bottom - ClientRect_Anim.top);

                f32 S_Width_Anim = 220.0f * GlobalScale; // Sidebar
                f32 L_Width_Anim = Width_Anim - S_Width_Anim - (Width_Anim * 0.3f < 300 * GlobalScale ? 300 * GlobalScale : Width_Anim * 0.3f);
                f32 SearchW_Anim = 320.0f * GlobalScale;
                f32 SearchX_Anim = S_Width_Anim + (L_Width_Anim - SearchW_Anim) * 0.5f;
                b32 SearchIsActive = GlobalSearchFocused || (GlobalMouseX >= SearchX_Anim && 
                                                           GlobalMouseX <= SearchX_Anim + SearchW_Anim &&
                                                           GlobalMouseY >= 6 * GlobalScale && GlobalMouseY <= 38 * GlobalScale);
                if(SearchIsActive) { if(GlobalSearchHoverT < 1.0f) GlobalSearchHoverT += 0.15f; }
                else { if(GlobalSearchHoverT > 0.0f) GlobalSearchHoverT -= 0.1f; }

                // Sidebar Hover Animations
                f32 HeaderHeight = 54.0f * GlobalScale;
                f32 SideY_Anim = HeaderHeight + (20.0f * GlobalScale);
                SideY_Anim += 32.0f * GlobalScale;
                for(int i=0; i<32; ++i) {
                    b32 Hover = (GlobalMouseX < S_Width_Anim && GlobalMouseY >= SideY_Anim && GlobalMouseY < SideY_Anim + 30.0f*GlobalScale);
                    if(Hover) { if(GlobalSidebarHoverT[i] < 1.0f) GlobalSidebarHoverT[i] += 0.15f; }
                    else { if(GlobalSidebarHoverT[i] > 0.0f) GlobalSidebarHoverT[i] -= 0.1f; }
                    SideY_Anim += 30.0f * GlobalScale;
                    if(i == 5) SideY_Anim += 56.0f * GlobalScale; // Gap
                }

                // Navigation Transition Animation
                GlobalNavTransitionT += (0.0f - GlobalNavTransitionT) * 0.12f; // Easing
                if(GlobalNavTransitionT < 0.001f) GlobalNavTransitionT = 0;
                
                // Render
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);
                f32 Width = (f32)(ClientRect.right - ClientRect.left);
                f32 Height = (f32)(ClientRect.bottom - ClientRect.top);

                BeginFrame(&GlobalRenderer, Width, Height);
                
                {
                    char TitleBuf[256];
                    sprintf_s(TitleBuf, 256, "Zenith");
                    SetWindowTextA(Window, TitleBuf);
                }
                
                // Forward Decls
                // --- Entry Point ---
                // Sidebar: Scaled
                f32 SidebarWidth = 220.0f * GlobalScale;
                f32 PreviewWidth = Width * 0.3f;
                if(PreviewWidth < 300 * GlobalScale) PreviewWidth = 300 * GlobalScale;
                
                // Responsive Logic Collapse
                if (Width < 900.0f * GlobalScale) {
                    PreviewWidth = 0.0f; // Squeeze Preview natively allowing search bar and list priority
                }
                
                f32 ListWidth = Width - SidebarWidth - PreviewWidth;
                if (ListWidth < 300.0f * GlobalScale) ListWidth = 300.0f * GlobalScale;
                
                // Main Areas
                f32 SidebarX = 0;
                f32 ListX = SidebarWidth;
                f32 PreviewX = SidebarWidth + ListWidth;
                
                // --- Colors (Modern Dark Theme) ---
                f32 BgSidebar[]    = {0.11f, 0.11f, 0.11f, 1.0f}; // #1e1e1e
                f32 BgList[]       = {0.15f, 0.15f, 0.15f, 1.0f}; // #252526
                f32 BgPreview[]    = {0.11f, 0.11f, 0.11f, 1.0f}; // #1e1e1e
                
                f32 ItemHover[]    = {0.22f, 0.22f, 0.23f, 1.0f}; // #37373d (List Hover)
                f32 ItemSelect[]   = {0.35f, 0.35f, 0.35f, 1.0f}; // Neutral Silver
                
                f32 SidebarHover[] = {0.18f, 0.18f, 0.18f, 1.0f}; // #2a2d2e (Sidebar Hover)
                
                f32 TextColor[]    = {0.90f, 0.90f, 0.90f, 1.0f}; // #cccccc
                f32 MutedColor[]   = {0.60f, 0.60f, 0.60f, 1.0f}; 
                f32 AccentColor[]  = {0.70f, 0.70f, 0.70f, 1.0f}; // Neutral Silver
                
                RenderQuad(&GlobalRenderer, SidebarX, 0, SidebarWidth, Height, BgSidebar);
                RenderQuad(&GlobalRenderer, ListX, 0, ListWidth, Height, BgList);
                RenderQuad(&GlobalRenderer, PreviewX, 0, PreviewWidth, Height, BgPreview);
                
                // Sidebar Separator
                f32 SepColor[] = {0.18f, 0.18f, 0.18f, 1.0f};
                RenderQuad(&GlobalRenderer, ListX, 0, 1, Height, SepColor); // Left Border
                RenderQuad(&GlobalRenderer, PreviewX, 0, 1, Height, SepColor); // Right Border

                // --- CONSOLIDATED HEADER (Navigation & Toolbar) ---
                // f32 HeaderHeight = 54.0f * GlobalScale; // Defined above
                f32 ColumnsHeight = 30.0f * GlobalScale;
                f32 ListContentTop = HeaderHeight + ColumnsHeight;
                // (Header Background & Back Button moved to Overlay)
                // (Sidebar Call removed - duplicate)

                // --- SCROLL UPDATE (Restored) ---
                f32 TargetS = GlobalTargetScrollY;
                GlobalScrollY += (TargetS - GlobalScrollY) * 0.2f;
                if(fabs(TargetS - GlobalScrollY) < 1.0f) GlobalScrollY = TargetS; // Snap
               
                // (Tools moved to Overlay)
                
                // (Sidebar moved to top restored)
                // --- HEADER & NAVIGATION ---
                // Rendered FIRST to ensure Input Priority and correct Layout Spacing
                f32 HeaderH = 54.0f * GlobalScale;
                
                // BACK BUTTON
                f32 BackBtnX = ListX + 12 * GlobalScale;
                f32 BackBtnY = (HeaderH - 28*GlobalScale) * 0.5f;
                f32 BackBtnSize = 28 * GlobalScale;
                b32 BackHot = (GlobalMouseX >= BackBtnX && GlobalMouseX <= BackBtnX+BackBtnSize && 
                               GlobalMouseY >= BackBtnY && GlobalMouseY <= BackBtnY+BackBtnSize);
                
                // Active Hit Test for Back (Works in ALL modes)
                if(BackHot && GlobalLeftClickOneShot) {
                    GlobalLeftClickOneShot = false;
                    // If showing analysis, go back to Normal view
                    if(GlobalViewMode == 1) GlobalViewMode = 0; 
                    else GoBack(); // Standard Navigation
                }
                
                // Header Background (Opaque to clip content behind)
                RenderQuad(&GlobalRenderer, ListX, 0, ListWidth, HeaderH, BgList);
                RenderQuad(&GlobalRenderer, ListX, HeaderH-1, ListWidth, 1, (f32[]){0.0f, 0.0f, 0.0f, 0.4f});
                
                // Render Icon
                Render8BitIcon(ICON_DRIVE_INTERNAL, BackBtnX + 2*GlobalScale, BackBtnY + 2*GlobalScale, GlobalScale);
                if(BackHot) RenderQuad(&GlobalRenderer, BackBtnX, BackBtnY, BackBtnSize, BackBtnSize, (f32[]){1,1,1,0.1f});
                
                // TOOLS / ANALYSIS BUTTON
                // Aligned right
                f32 VizBtnW = 110*GlobalScale;
                f32 VizBtnH = 26*GlobalScale;
                f32 VizBtnX = ListX + ListWidth - VizBtnW - 20*GlobalScale;
                f32 VizBtnY = (HeaderH - VizBtnH) * 0.5f; // Center vertically in header
                
                b32 VizHot = (GlobalMouseX >= VizBtnX && GlobalMouseX <= VizBtnX+VizBtnW && GlobalMouseY >= VizBtnY && GlobalMouseY <= VizBtnY+VizBtnH);
                if(VizHot && GlobalLeftClickOneShot) {
                    // Toggle Storage Mode
                    if(GlobalViewMode == 0) {
                        GlobalViewMode = 1;
                        if(!GlobalStorageTree && !GlobalIsAnalyzing) {
                             GlobalIsAnalyzing = true;
                             wsprintfW(GlobalAnalysisPath, L"%s", GlobalCurrentPath);
                             if(GlobalAnalysisPath[0] == 0) wsprintfW(GlobalAnalysisPath, L""); // Handle Root Drives
                             CreateThread(0, 0, StorageAnalysisThread, 0, 0, 0);
                        }
                    }
                    else GlobalViewMode = 0;
                    GlobalStorageVisualizeMode = false; // Reset sub-mode
                    GlobalLeftClickOneShot = false;
                }
                
                f32 VizCol[] = {0.25f, 0.25f, 0.25f, 1.0f};
                if(GlobalViewMode == 1) { VizCol[0]=0.0f; VizCol[1]=0.45f; VizCol[2]=0.85f; } // Blue when active
                else if(VizHot) { VizCol[0]=0.35f; VizCol[1]=0.35f; VizCol[2]=0.35f; }
                
                RenderRoundedQuad(&GlobalRenderer, VizBtnX, VizBtnY, VizBtnW, VizBtnH, 4*GlobalScale, VizCol);
                DrawTextStr(&GlobalRenderer, &Font, VizBtnX + 15*GlobalScale, VizBtnY + 5*GlobalScale, GlobalViewMode == 1 ? "Close Analysis" : "Analyze Disk", (f32[]){1,1,1,1});

                // SEARCH BAR (Moved to Header)
                f32 SearchBaseW = 280.0f * GlobalScale;
                if (Width < 900.0f * GlobalScale) SearchBaseW = 120.0f * GlobalScale;
                f32 SearchW = SearchBaseW + (80.0f * GlobalScale * GlobalSearchHoverT);
                f32 SearchH = 32.0f * GlobalScale;
                // Anchor left of Analyze Button
                f32 SearchX = VizBtnX - SearchW - 20 * GlobalScale;
                f32 SearchY = (HeaderH - SearchH) * 0.5f;
                b32 SearchHot = (GlobalMouseX >= SearchX && GlobalMouseX <= SearchX+SearchW && GlobalMouseY >= SearchY && GlobalMouseY <= SearchY+SearchH);
                if(SearchHot) GlobalSearchHot = true; 
                else GlobalSearchHot = false;
                
                if(GlobalSearchHot && GlobalLeftClickOneShot) { GlobalSearchFocused = true; GlobalLeftClickOneShot = false; }
                
                f32 SBarCol[4] = {0.06f, 0.06f, 0.06f, 1.0f};
                if(GlobalSearchFocused) { SBarCol[0] = 0.15f; SBarCol[1] = 0.15f; SBarCol[2] = 0.15f; } 
                else if(GlobalSearchHoverT > 0.01f) { f32 H = GlobalSearchHoverT; SBarCol[0] += 0.06f * H; SBarCol[1] += 0.06f * H; SBarCol[2] += 0.06f * H; }
                RenderRoundedQuad(&GlobalRenderer, SearchX, SearchY, SearchW, SearchH, 6*GlobalScale, SBarCol); 
                f32 BorderCol[] = {0.3f, 0.3f, 0.3f, 0.5f};
                if(GlobalSearchFocused) { BorderCol[0]=0.5f; BorderCol[1]=0.5f; BorderCol[2]=0.5f; BorderCol[3]=1.0f; }
                RenderQuad(&GlobalRenderer, SearchX, SearchY, SearchW, 1, BorderCol); RenderQuad(&GlobalRenderer, SearchX, SearchY+SearchH-1, SearchW, 1, BorderCol);
                RenderQuad(&GlobalRenderer, SearchX, SearchY, 1, SearchH, BorderCol); RenderQuad(&GlobalRenderer, SearchX+SearchW-1, SearchY, 1, SearchH, BorderCol);
                f32 IconX = SearchX + 10 * GlobalScale; f32 IconY = SearchY + (SearchH - 14*GlobalScale) * 0.5f;
                Render8BitIcon(ICON_SEARCH, IconX, IconY, GlobalScale * 0.85f);
                f32 TextX = SearchX + 36 * GlobalScale; f32 TextY = SearchY + (SearchH - 18*GlobalScale) * 0.5f;
                if(!GlobalSearchBuffer[0] && !GlobalSearchFocused) DrawTextStr(&GlobalRenderer, &Font, TextX, TextY, "Discovery...", (f32[]){0.5f, 0.5f, 0.5f, 0.5f});
                else { f32 TCol[] = {0.9f, 0.9f, 0.9f, 1.0f}; DrawTextStr(&GlobalRenderer, &Font, TextX, TextY, GlobalSearchBuffer, TCol);
                    if(GlobalSearchFocused && (((int)(GlobalTime * 2.2f)) % 2 == 0)) { f32 TextW = (f32)lstrlenA(GlobalSearchBuffer) * 9.5f * GlobalScale; RenderQuad(&GlobalRenderer, TextX + TextW, TextY + 2*GlobalScale, 2, SearchH - 20*GlobalScale, (f32[]){0.8f, 0.8f, 0.8f, 0.8f}); }
                }

                RenderSidebar(&GlobalRenderer, &Font, SidebarX, HeaderH, SidebarWidth, Height - HeaderH, GlobalMouseX, GlobalMouseY, SidebarHover, TextColor, MutedColor, GlobalSidebarHoverT);
                
                // --- SCAN PROGRESS BAR (Status Area) ---
                u32 Total = GlobalWorkQueue.TotalJobsCount;
                u32 Completed = GlobalWorkQueue.CompletedJobsCount;
                if(Total > 0)
                {
                    f32 Pct = (f32)Completed / (f32)Total;
                    if(Pct > 1.0f) Pct = 1.0f;
                    
                    f32 ProgW = SidebarWidth - 24 * GlobalScale;
                    f32 ProgH = 4 * GlobalScale;
                    f32 ProgX = SidebarX + 12 * GlobalScale;
                    f32 ProgY = Height - 50 * GlobalScale;
                    
                    // MOVED TO END
                    /*
                    RenderQuad(&GlobalRenderer, ProgX, ProgY, ProgW, ProgH, (f32[]){0.15f, 0.15f, 0.15f, 1.0f});
                    RenderQuad(&GlobalRenderer, ProgX, ProgY, ProgW * Pct, ProgH, (f32[]){0.7f, 0.7f, 0.7f, 1.0f}); // Silver Fill
                    DrawTextStr(&GlobalRenderer, &Font, ProgX, ProgY - 18*GlobalScale, "Discovery Indexing...", (f32[]){0.6f, 0.6f, 0.6f, 1});
                    */
                }

                // Helper: Load Preview if needed
           // RE-ENABLED (Text Only Test)
                if(!GlobalPreviewLoaded && GlobalSelectedIndex >= 0 && GlobalSelectedIndex < (int)GlobalDir.Count)
                {
                    file_info *File = GlobalDir.Files + GlobalSelectedIndex;
                    wchar_t FullPath[MAX_PATH];
                    int P_Len = lstrlenW(GlobalCurrentPath);
                    if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(FullPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                    else wsprintfW(FullPath, L"%s%s", GlobalCurrentPath, File->FileName);

                    if(!File->IsDirectory)
                    {
                        // Release Old Texture
                        if(GlobalPreviewTexture) { GlobalPreviewTexture->lpVtbl->Release(GlobalPreviewTexture); GlobalPreviewTexture = 0; }

                        // Release Old COM Preview
                        if(GlobalPreviewHandler) {
                            GlobalPreviewHandler->lpVtbl->Unload(GlobalPreviewHandler);
                            GlobalPreviewHandler->lpVtbl->Release(GlobalPreviewHandler);
                            GlobalPreviewHandler = 0;
                        }

                        // Check Extension
                        b32 IsImage = false;
                        b32 IsNativeDoc = false;
                        wchar_t *Dot = wcsrchr(File->FileName, '.');
                        if(Dot)
                        {
                            if(lstrcmpiW(Dot, L".png")==0 || lstrcmpiW(Dot, L".jpg")==0 ||
                               lstrcmpiW(Dot, L".jpeg")==0 || lstrcmpiW(Dot, L".bmp")==0 ||
                               lstrcmpiW(Dot, L".gif")==0 || lstrcmpiW(Dot, L".tiff")==0 ||
                               lstrcmpiW(Dot, L".ico")==0 || lstrcmpiW(Dot, L".wdp")==0) IsImage = true;

                            if(lstrcmpiW(Dot, L".pdf")==0 || lstrcmpiW(Dot, L".docx")==0 ||
                               lstrcmpiW(Dot, L".doc")==0 || lstrcmpiW(Dot, L".xlsx")==0 ||
                               lstrcmpiW(Dot, L".xls")==0 || lstrcmpiW(Dot, L".pptx")==0 ||
                               lstrcmpiW(Dot, L".ppt")==0 || lstrcmpiW(Dot, L".rtf")==0) IsNativeDoc = true;
                        }

                        if(IsNativeDoc)
                        {
                            IShellItem *pItem = NULL;
                            HRESULT hr = SHCreateItemFromParsingName(FullPath, NULL, &IID_IShellItem, (void**)&pItem);
                            if(SUCCEEDED(hr) && pItem)
                            {
                                hr = pItem->lpVtbl->BindToHandler(pItem, NULL, &BHID_PreviewHandler_Custom, &IID_IPreviewHandler, (void**)&GlobalPreviewHandler);
                                if(SUCCEEDED(hr) && GlobalPreviewHandler)
                                {
                                    RECT rc;
                                    GetClientRect(GlobalMainWindow, &rc);
                                    RECT prc;
                                    float P_W = (float)(rc.right - rc.left) * 0.3f;
                                    if(P_W < 300) P_W = 300;
                                    prc.top = 80;
                                    prc.bottom = rc.bottom;
                                    prc.right = rc.right;
                                    prc.left = rc.right - (int)P_W;
                                    
                                    GlobalPreviewHandler->lpVtbl->SetWindow(GlobalPreviewHandler, GlobalMainWindow, &prc);
                                    
                                    IInitializeWithItem *pInitItem = NULL;
                                    hr = GlobalPreviewHandler->lpVtbl->QueryInterface(GlobalPreviewHandler, &IID_IInitializeWithItem, (void**)&pInitItem);
                                    if(SUCCEEDED(hr)) {
                                        pInitItem->lpVtbl->Initialize(pInitItem, pItem, STGM_READ);
                                        pInitItem->lpVtbl->Release(pInitItem);
                                    } else {
                                        IInitializeWithFile *pInitFile = NULL;
                                        hr = GlobalPreviewHandler->lpVtbl->QueryInterface(GlobalPreviewHandler, &IID_IInitializeWithFile, (void**)&pInitFile);
                                        if(SUCCEEDED(hr)) {
                                            pInitFile->lpVtbl->Initialize(pInitFile, FullPath, STGM_READ);
                                            pInitFile->lpVtbl->Release(pInitFile);
                                        }
                                    }
                                    
                                    GlobalPreviewHandler->lpVtbl->SetRect(GlobalPreviewHandler, &prc);
                                    GlobalPreviewHandler->lpVtbl->DoPreview(GlobalPreviewHandler);
                                    SetFocus(GlobalMainWindow); // Return keyboard focus to our window
                                }
                                pItem->lpVtbl->Release(pItem);
                            }
                        }

                        if(IsImage)
                        {
                             // Re-Enabled WIC
                             GlobalPreviewTexture = LoadTextureFromWIC(&GlobalRenderer, FullPath);
                        }

                        // Read File Content (Text/Hex Preview backup)
                        b32 Success = false;
                        u32 Bytes = 0;
                        if (!IsNativeDoc)
                        {
                            Bytes = ReadFileChunk(FullPath, GlobalPreviewBuffer, sizeof(GlobalPreviewBuffer)-1, &Success);
                        }
                        else
                        {
                            Success = true; // Skip reading to text buffer
                        }
                        GlobalPreviewBuffer[Bytes] = 0;
                        GlobalPreviewDir.Count = 0; // Clear dir preview
                        
                        if(!Success)
                        {
                            // Error Handling
                            DWORD Error = GetLastError();
                            if(Error == ERROR_ACCESS_DENIED)
                                sprintf_s(GlobalPreviewBuffer, sizeof(GlobalPreviewBuffer), "Error: Access Denied (System File).");
                            else if(Error == ERROR_SHARING_VIOLATION)
                                sprintf_s(GlobalPreviewBuffer, sizeof(GlobalPreviewBuffer), "Error: File in use by another process.");
                            else
                                sprintf_s(GlobalPreviewBuffer, sizeof(GlobalPreviewBuffer), "Error reading file (Code: %d).", Error);
                        }
                        else
                        {
                            // Binary Check -> Hex View (Only if not Image?)
                            // If Image, we still might want to see size or something. 
                            // But usually if Image, we show Image.
                            // If load failed (GlobalPreviewTexture == 0), shown Hex/Text?
                            
                            b32 IsBinary = false;
                            for(u32 b=0; b<Bytes && b<500; ++b) { if(GlobalPreviewBuffer[b] == 0) { IsBinary = true; break; } }
                            
                            if(IsBinary && !GlobalPreviewTexture)
                            {
                                char HexOut[4096];
                                char *Dst = HexOut;
                                int Rem = sizeof(HexOut);
                                Dst += sprintf_s(Dst, Rem, "Binary File (Hex View):\n");
                                Rem -= (int)(Dst - HexOut);
                                
                                u8 *Src = (u8 *)GlobalPreviewBuffer;
                                for(int Line=0; Line<16 && Line*16 < (int)Bytes; ++Line)
                                {
                                    Dst += sprintf_s(Dst, Rem, "%04X: ", Line*16);
                                    Rem = sizeof(HexOut) - (int)(Dst-HexOut);
                                    for(int b=0; b<16; ++b)
                                    {
                                        if(Line*16 + b < (int)Bytes) Dst += sprintf_s(Dst, Rem, "%02X ", Src[Line*16 + b]);
                                        else Dst += sprintf_s(Dst, Rem, "   ");
                                        Rem = sizeof(HexOut) - (int)(Dst-HexOut);
                                    }
                                    Dst += sprintf_s(Dst, Rem, " | ");
                                    Rem = sizeof(HexOut) - (int)(Dst-HexOut);
                                    for(int b=0; b<16; ++b)
                                    {
                                        if(Line*16 + b < (int)Bytes) {
                                            u8 c = Src[Line*16+b];
                                            Dst += sprintf_s(Dst, Rem, "%c", (c>=32 && c<=126)?c:'.');
                                        }
                                        Rem = sizeof(HexOut) - (int)(Dst-HexOut);
                                    }
                                    Dst += sprintf_s(Dst, Rem, "\n");
                                    Rem = sizeof(HexOut) - (int)(Dst-HexOut);
                                }
                                // FIX: Use strcpy_s or copy specific length, NOT sizeof(GlobalPreviewBuffer) which is 64k vs HexOut 4k
                                strcpy_s(GlobalPreviewBuffer, sizeof(GlobalPreviewBuffer), HexOut);
                            }
                        }
                    }
                    else
                    {
                        // Directory Preview
                        GlobalPreviewBuffer[0] = 0; 
                        GlobalPreviewArena.Used = 0; 
                        GlobalPreviewDir = ScanDirectory(&GlobalPreviewArena, FullPath);
                        SortFiles(&GlobalPreviewDir, 0, true); 
                    }
                    GlobalPreviewLoaded = true;
                }
                


                
                
                // --- Preview Pane ---
                if(GlobalSelectedIndex >= 0)
                {
                    Inspector_Draw(&GlobalRenderer, &Font, PreviewX, 0, PreviewWidth, Height, GlobalMouseX, GlobalMouseY, TextColor, MutedColor, SepColor, &GlobalPreviewDir);
                }
                
                GlobalListTop = ListContentTop; 
                
                if(GlobalIsDragging)
                {
                    f32 X = (f32)(GlobalDragStartX < GlobalDragEndX ? GlobalDragStartX : GlobalDragEndX);
                    f32 Y = (f32)(GlobalDragStartY < GlobalDragEndY ? GlobalDragStartY : GlobalDragEndY);
                    f32 W = (f32)abs(GlobalDragEndX - GlobalDragStartX);
                    f32 H = (f32)abs(GlobalDragEndY - GlobalDragStartY);
                    
                    if(W > 2 && H > 2)
                    {
                        f32 BoxColor[] = {0.0f, 0.48f, 0.8f, 0.2f};
                        f32 BorderColor[] = {0.0f, 0.48f, 0.8f, 0.8f};
                        
                        RenderQuad(&GlobalRenderer, X, Y, W, H, BoxColor);
                        RenderQuad(&GlobalRenderer, X, Y, W, 1, BorderColor);
                        RenderQuad(&GlobalRenderer, X, Y+H, W, 1, BorderColor);
                        RenderQuad(&GlobalRenderer, X, Y, 1, H, BorderColor);
                        RenderQuad(&GlobalRenderer, X+W, Y, 1, H, BorderColor);
                    }
                } 
                
                f32 StatusH = 32.0f * GlobalScale;
                f32 StatusY = Height - StatusH;
                
                // --- Header ---
                // --- Header ---
                
                // --- Main List Area ---
                // We use GlobalViewMode to strictly separate the two views.
                // This prevents "competition" and "double text".
                
                if(GlobalViewMode == 1) // STORAGE VIEW
                {
                    GlobalRenderer.GlobalAlpha = 1.0f; // FORCE ALPHA
                    GlobalRenderer.GlobalOffsetX = 0;
                    GlobalRenderer.GlobalOffsetY = 0;

                    // Draw a background to fully obscure the main list area
                    RenderQuad(&GlobalRenderer, ListX, HeaderHeight, ListWidth, Height - HeaderHeight, BgList);



                    // Title / Analysis Progress
                    // Title / Analysis Progress
                    if(GlobalIsAnalyzing) {
                         f32 SpinX = ListX + (ListWidth * 0.5f) - 20;
                         f32 SpinY = HeaderHeight + 80; // Center in avail space
                         RenderQuad(&GlobalRenderer, SpinX, SpinY, 40, 40, (f32[]){0.3f, 0.3f, 0.3f, 1}); 
                         DrawTextStr(&GlobalRenderer, &Font, SpinX - 40, SpinY + 50, "Scanning files...", MutedColor);
                    }
                    else if(GlobalStorageVisualizeMode) {
                         // Visualization View
                         RenderStorageView(ListX, HeaderHeight + 30.0f, ListWidth, StatusY - (HeaderHeight + 30.0f), GlobalStorageTree, GlobalScrollY, GlobalMouseX, GlobalMouseY, GlobalLeftClickOneShot);
                    } 
                    else {
                         // Standard Tree View
                         if(!GlobalStorageTree) {
                             DrawTextStr(&GlobalRenderer, &Font, ListX + 50, HeaderHeight + 50, "Scanner Ready. Click 'Analyze Disk' to begin.", MutedColor);
                         } else {
                             RenderStorageView(ListX, HeaderHeight + 30.0f, ListWidth, StatusY - (HeaderHeight + 30.0f), GlobalStorageTree, GlobalScrollY, GlobalMouseX, GlobalMouseY, GlobalLeftClickOneShot);
                         }
                    }
                    
                    if(GlobalLeftClickOneShot) GlobalLeftClickOneShot = false; 
                }
                else // STANDARD FILE LIST
                {
                    GlobalRenderer.GlobalAlpha = 1.0f - ViewAnim;
                    f32 HeaderH = HeaderHeight;
                    
                    if(GlobalCurrentPath[0] == 0) // Home Page
                    {
                        f32 NavX = SidebarWidth;
                        if(GlobalNavTransitionT > 0) {
                            f32 Offset = (GlobalNavDirection == 0 ? 40.0f : -40.0f) * GlobalNavTransitionT * GlobalScale;
                            GlobalRenderer.GlobalOffsetX = Offset;
                            GlobalRenderer.GlobalAlpha = (1.0f - GlobalNavTransitionT);
                        }
                        RenderHomeView(NavX, HeaderH + 20*GlobalScale, ListWidth, StatusY - (HeaderH + 20*GlobalScale), GlobalLeftClickOneShot);
                        if(GlobalLeftClickOneShot) GlobalLeftClickOneShot = false;
                        GlobalRenderer.GlobalOffsetX = 0;
                        GlobalRenderer.GlobalAlpha = 1.0f;
                    }
                    else
                    {
                        // Navigation Animation Offset
                        if(GlobalNavTransitionT > 0) {
                            f32 Offset = (GlobalNavDirection == 0 ? 40.0f : -40.0f) * GlobalNavTransitionT * GlobalScale;
                            GlobalRenderer.GlobalOffsetX = Offset;
                            GlobalRenderer.GlobalAlpha = (1.0f - GlobalNavTransitionT);
                        }

                        // Header Labels
                        f32 LabelsY = HeaderH + 10;
                        HeaderH += 30 * GlobalScale; // Spacing for labels (already drawn in header)

                        // File List
                        f32 Y = HeaderH - GlobalScrollY;
                        f32 RowH = GlobalRowHeight;
                        
                        // Clip Area: StatusY
                        
                        int Count = GlobalDir.Count;
                        if(GlobalDir.Files)
                        {
                            for(int i=0; i<Count; ++i)
                            {
                                if(Y + RowH < HeaderH) { Y += RowH; continue; } // Clip Top
                                if(Y > StatusY) break; // Clip Bottom at Status Bar
                                
                                file_info *File = GlobalDir.Files + i;
                            
                            // Selection highlight
                            if(i == GlobalSelectedIndex) {
                                f32 SelectCol[] = {0.0f, 0.4f, 0.8f, 0.5f};
                                RenderQuad(&GlobalRenderer, SidebarWidth, Y, ListWidth, RowH, SelectCol);
                            }
                            
                            // Hover Animation: 
                            // We use a per-row timer (GlobalListHoverT) to lerp transparency.
                            // This creates a smooth "fade in/out" transition instead of a hard toggle.
                            b32 IsHover = (GlobalMouseX >= SidebarWidth && GlobalMouseX < SidebarWidth + ListWidth &&
                                           GlobalMouseY >= Y && GlobalMouseY < Y + RowH && !GlobalContextMenuActive);
                            
                            if(i < 1024) {
                                f32 TargetT = IsHover ? 1.0f : 0.0f;
                                GlobalListHoverT[i] += (TargetT - GlobalListHoverT[i]) * 0.2f; // Animation speed
                                if(GlobalListHoverT[i] > 0.01f) {
                                    f32 HoverAlpha = 0.05f * GlobalListHoverT[i];
                                    f32 HoverCol[] = {1.0f, 1.0f, 1.0f, HoverAlpha};
                                    RenderQuad(&GlobalRenderer, SidebarWidth, Y, ListWidth, RowH, HoverCol);
                                }
                            }

                            // 8-Bit Icon
                            icon_type IType = DetermineIconType(File->FileName, File->IsDirectory);
                            Render8BitIcon(IType, SidebarWidth + (10 * GlobalScale), Y + (4 * GlobalScale), GlobalScale);
                            
                            // Text
                            // Text
                            char Name[256];
                            wsprintfA(Name, "%S", File->FileName);
                            
                            // Truncation Logic for Overlap
                            f32 NameX = SidebarWidth + (45 * GlobalScale);
                            f32 DateX = SidebarWidth + ListWidth - (250 * GlobalScale); // Date Column Start
                            f32 MaxNameW = DateX - NameX - (10 * GlobalScale); // 10px Padding
                            
                            if(MaxNameW > 0) {
                                int NameLen = lstrlenA(Name);
                                f32 EstNameW = NameLen * (9.0f * GlobalScale); // Approx width
                                
                                if(EstNameW > MaxNameW) {
                                     int MaxC = (int)(MaxNameW / (9.0f * GlobalScale));
                                     if(MaxC < 5) MaxC = 5;
                                     if(NameLen > MaxC) {
                                         Name[MaxC] = 0; // Hard cut first
                                         if(MaxC > 3) {
                                             Name[MaxC-1] = '.'; Name[MaxC-2] = '.'; Name[MaxC-3] = '.';
                                         }
                                     }
                                }
                            }

                            DrawTextStr(&GlobalRenderer, &Font, NameX, Y + (8 * GlobalScale), Name, (f32[]){1,1,1,1});
                            
                            DrawTextStr(&GlobalRenderer, &Font, SidebarWidth + ListWidth - (250 * GlobalScale), Y + (8 * GlobalScale), File->DateString, (f32[]){0.7f, 0.7f, 0.7f, 1});
                            
                            char SizeBuf[64] = {0};
                            char *SizeToDisplay = File->SizeString;
                            
                            if(File->IsDirectory)
                            {
                                wchar_t FullPath[MAX_PATH];
                                int P_Len = lstrlenW(GlobalCurrentPath);
                                if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(FullPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                                else wsprintfW(FullPath, L"%s%s", GlobalCurrentPath, File->FileName);

                                u64 Cached = GetCachedSize(FullPath);
                                if(Cached != (u64)-1 && Cached != (u64)-2)
                                {
                                    double S = (double)Cached;
                                    if(S >= 1024.0*1024.0*1024.0) sprintf_s(SizeBuf, 64, "%.1f GB", S/(1024.0*1024.0*1024.0));
                                    else if(S >= 1024.0*1024.0) sprintf_s(SizeBuf, 64, "%.1f MB", S/(1024.0*1024.0));
                                    else if(S >= 1024.0) sprintf_s(SizeBuf, 64, "%.1f KB", S/1024.0);
                                    else sprintf_s(SizeBuf, 64, "%.0f B", S);
                                    SizeToDisplay = SizeBuf;
                                    SizeToDisplay = SizeBuf;
                                } else {
                                    // Fallback to blank if cache missing
                                    sprintf_s(SizeBuf, 64, "...");
                                    SizeToDisplay = SizeBuf;
                                }
                            }
                            
                            DrawTextStr(&GlobalRenderer, &Font, SidebarWidth + ListWidth - (120 * GlobalScale), Y + (8 * GlobalScale), SizeToDisplay, (f32[]){0.7f, 0.7f, 0.7f, 1});
                            
                            Y += RowH;
                        }
                    }
                }
                
                GlobalRenderer.GlobalOffsetX = 0;
                GlobalRenderer.GlobalAlpha = 1.0f;
                
                // Status Text
                char StatusBuf[256];
                int Selected = (GlobalSelectedIndex >= 0) ? 1 : 0; 
                
                extern search_index GlobalIndex;
                if(GlobalIndex.IsBuilding) {
                    sprintf_s(StatusBuf, sizeof(StatusBuf), "INDEXING... %d items   |   %d items   %d selected", GlobalIndex.Count, GlobalDir.Count, Selected);
                } else {
                    sprintf_s(StatusBuf, sizeof(StatusBuf), "INDEX: %d items   |   %d items   %d selected", GlobalIndex.Count, GlobalDir.Count, Selected);
                }
                // MOVED TO END
                // DrawTextStr(&GlobalRenderer, &Font, 10 * GlobalScale, StatusY + (8 * GlobalScale), StatusBuf, MutedColor);
                
                f32 BackCol[] = {0.2f, 0.2f, 0.2f, 1.0f};
                if(BackHot) BackCol[0] = 0.3f;
                RenderRoundedQuad(&GlobalRenderer, BackBtnX, BackBtnY, BackBtnSize, BackBtnSize, 6*GlobalScale, BackCol);
                DrawTextStr(&GlobalRenderer, &Font, BackBtnX + 9*GlobalScale, BackBtnY + 6*GlobalScale, "<", TextColor);
                
                // Tools
                f32 ToolPadding = 12 * GlobalScale;
                f32 CurrentToolX = ListX + ListWidth - ToolPadding;
                f32 RefSize = 28 * GlobalScale;
                CurrentToolX -= RefSize;
                f32 RefX = CurrentToolX;
                // Deleted Ghost Buttons (Ref, Storage, Search)

                if(GlobalViewMode == 0) {
                    f32 ColY = HeaderHeight;
                    f32 ColBg[] = {0.13f, 0.13f, 0.13f, 1.0f};
                    RenderQuad(&GlobalRenderer, ListX, ColY, ListWidth, ColumnsHeight, ColBg);
                    RenderQuad(&GlobalRenderer, ListX, ColY + ColumnsHeight - 1, ListWidth, 1, (f32[]){0.18f, 0.18f, 0.18f, 1.0f});
                    f32 ColNameX = ListX + 44; f32 ColDateX = ListX + ListWidth - 210; f32 ColSizeX = ListX + ListWidth - 80;
                    if(GlobalLeftClickOneShot)
                    {
                         // Hit Test for Headers
                         if(GlobalMouseY >= ColY && GlobalMouseY <= ColY + ColumnsHeight)
                         {
                             int NewMode = -1;
                             // Widths logic mirrors the rendering below
                             if(GlobalMouseX > ColSizeX) NewMode = 2; // Size
                             else if(GlobalMouseX > ColDateX) NewMode = 1; // Date
                             else NewMode = 0; // Name
                             
                             if(NewMode != -1)
                             {
                                 if(NewMode == GlobalSortMode) GlobalSortAsc = !GlobalSortAsc;
                                 else { GlobalSortMode = NewMode; GlobalSortAsc = true; }
                                 
                                 SortFiles(&GlobalDir, GlobalSortMode, GlobalSortAsc);
                                 GlobalLeftClickOneShot = false; // Consume
                             }
                         }
                    }
                    RenderQuad(&GlobalRenderer, ColDateX - 10, ColY + 5, 1, ColumnsHeight - 10, SepColor);
                    RenderQuad(&GlobalRenderer, ColSizeX - 10, ColY + 5, 1, ColumnsHeight - 10, SepColor);
                    DrawTextStr(&GlobalRenderer, &Font, ColNameX, ColY + (5 * GlobalScale), GlobalSortMode == 0 ? (GlobalSortAsc ? "Name \30" : "Name \31") : "Name", MutedColor);
                    DrawTextStr(&GlobalRenderer, &Font, ColDateX, ColY + (5 * GlobalScale), GlobalSortMode == 1 ? (GlobalSortAsc ? "Date \30" : "Date \31") : "Date Modified", MutedColor);
                    DrawTextStr(&GlobalRenderer, &Font, ColSizeX, ColY + (5 * GlobalScale), GlobalSortMode == 2 ? (GlobalSortAsc ? "Size \30" : "Size \31") : "Size", MutedColor);
                }

                // Context Menu Overlay
                if(GlobalContextMenuActive)
                {
                    // Animation (Lerp Scale & Alpha)
                    f32 TargetScale = 1.0f;
                    GlobalMenuScale += (TargetScale - GlobalMenuScale) * 0.2f; 
                    if(GlobalMenuScale > 0.99f) GlobalMenuScale = 1.0f;
                    
                    GlobalMenuAlpha = GlobalMenuScale; // Sync Alpha to Scale for pop effect

                    f32 MenuW = 200.0f * GlobalScale;
                    // Calculate Height based on Matches
                    menu_item AllItems[] = { {"Open", 0}, {"Copy Path", 1}, {"Properties", 2}, {"Calc Folder Size", 3}, 
                                             {"Delete", 4}, {"Reveal in Explorer", 5}, {"Open Terminal", 6} };
                    int MatchCount = 0;
                    int Matches[10];
                    for(int m=0; m<7; ++m) {
                        if(!GlobalMenuSearch[0] || CaseInsensitiveContains(AllItems[m].Name, GlobalMenuSearch)) 
                             Matches[MatchCount++] = m;
                    }

                    f32 MenuH = (40.0f + (MatchCount * 24.0f) + 10.0f) * GlobalScale;
                    
                    // Apply Scale Transform relative to Top-Left (Mouse)
                    f32 ScaledW = MenuW * GlobalMenuScale;
                    f32 ScaledH = MenuH * GlobalMenuScale;
                    
                    f32 MenuX = (f32)GlobalContextMenuX;
                    // Vertical Offset for "slide down" effect
                    f32 MenuY = (f32)GlobalContextMenuY + (10.0f * (1.0f - GlobalMenuScale));
                    
                    // Shadow
                    f32 ShadowColor[] = {0,0,0,0.4f * GlobalMenuAlpha};
                    RenderQuad(&GlobalRenderer, MenuX + (4 * GlobalScale), MenuY + (4 * GlobalScale), ScaledW, ScaledH, ShadowColor);
            
                    f32 MenuBg[] = {0.2f, 0.2f, 0.2f, 1.0f * GlobalMenuAlpha};
                    f32 MenuBorder[] = {0.3f, 0.3f, 0.3f, 1.0f * GlobalMenuAlpha};
                    
                    RenderQuad(&GlobalRenderer, MenuX, MenuY, ScaledW, ScaledH, MenuBorder);
                    RenderQuad(&GlobalRenderer, MenuX+1, MenuY+1, ScaledW-2, ScaledH-2, MenuBg);
                    
                    if(GlobalMenuScale > 0.8f) // Only draw content when almost full size
                    {
                        // Search Input
                        f32 SearchBg[] = {0.1f, 0.1f, 0.1f, 1.0f};
                        RenderQuad(&GlobalRenderer, MenuX + (5 * GlobalScale), MenuY + (5 * GlobalScale), MenuW - (10 * GlobalScale), 24 * GlobalScale, SearchBg);
                        char MenuDisp[64];
                        if(GlobalMenuSearch[0]) sprintf_s(MenuDisp, sizeof(MenuDisp), "%s", GlobalMenuSearch);
                        else sprintf_s(MenuDisp, sizeof(MenuDisp), "Filter...");
                        DrawTextStr(&GlobalRenderer, &Font, MenuX + (10 * GlobalScale), MenuY + (8 * GlobalScale), MenuDisp, MutedColor);
                        
                        f32 ItemY = MenuY + (35 * GlobalScale);
                        f32 HoverColor[] = {0.0f, 0.48f, 0.8f, 1.0f};
                        
                        for(int i=0; i<MatchCount; ++i)
                        {
                            int Id = Matches[i];
                            b32 IsHover = (GlobalMouseX >= MenuX && GlobalMouseX <= MenuX+MenuW &&
                                           GlobalMouseY >= ItemY && GlobalMouseY < ItemY + (24 * GlobalScale));
                            
                            if(IsHover) RenderQuad(&GlobalRenderer, MenuX+2, ItemY, MenuW-4, 24 * GlobalScale, HoverColor);
                            DrawTextStr(&GlobalRenderer, &Font, MenuX + (12 * GlobalScale), ItemY + (4 * GlobalScale), AllItems[Id].Name, TextColor);
                            ItemY += 24 * GlobalScale;
                        }
                    }
                }
                else { GlobalMenuAlpha = 0.0f; GlobalMenuScale = 0.0f; } // Reset on Close

                } // End File List / Storage View Else Block

                // --- TOP LAYER: Status Bar & Progress ---
                GlobalRenderer.GlobalAlpha = 1.0f; // RESET ALPHA for top layer
                GlobalRenderer.GlobalOffsetX = 0;
                GlobalRenderer.GlobalOffsetY = 0;
                // Redeclare to fix scope/shadowing
                StatusH = 30.0f * GlobalScale;
                StatusY = Height - StatusH;
                
                // We render this last to ensure it's always on top of list items
                f32 StatusBg[] = {0.15f, 0.15f, 0.15f, 1.0f}; 
                RenderQuad(&GlobalRenderer, 0, StatusY, Width, StatusH, StatusBg);

                // --- TEXT & PROGRESS (Moved here for Z-Order) ---
                // 1. Progress Bar (If Active)
                Total = GlobalWorkQueue.TotalJobsCount;
                Completed = GlobalWorkQueue.CompletedJobsCount;
                if(Total > 0)
                {
                    f32 Pct = (f32)Completed / (f32)Total;
                    if(Pct > 1.0f) Pct = 1.0f;
                    
                    f32 ProgW = SidebarWidth - 24 * GlobalScale;
                    f32 ProgH = 4 * GlobalScale;
                    f32 ProgX = SidebarX + 12 * GlobalScale;
                    f32 ProgY = Height - 40 * GlobalScale; // Adjust UP slightly
                    
                    RenderQuad(&GlobalRenderer, ProgX, ProgY, ProgW, ProgH, (f32[]){0.15f, 0.15f, 0.15f, 1.0f});
                    RenderQuad(&GlobalRenderer, ProgX, ProgY, ProgW * Pct, ProgH, (f32[]){0.7f, 0.7f, 0.7f, 1.0f}); 
                    DrawTextStr(&GlobalRenderer, &Font, ProgX, ProgY - 18*GlobalScale, "Discovery Indexing...", (f32[]){0.6f, 0.6f, 0.6f, 1});
                }

                // 2. Status Text
                char StatusIdx[128];
                char StatusIms[128];
                int Selected = (GlobalSelectedIndex >= 0) ? 1 : 0; 
                extern search_index GlobalIndex;
                
                if(GlobalIndex.IsBuilding) {
                     sprintf_s(StatusIdx, sizeof(StatusIdx), "IDX: %llu [INDEXING...]", GlobalIndex.TotalIndexedCount + GlobalIndex.Count);
                } else {
                     sprintf_s(StatusIdx, sizeof(StatusIdx), "IDX: %llu", GlobalIndex.TotalIndexedCount + GlobalIndex.Count);
                }
                sprintf_s(StatusIms, sizeof(StatusIms), "Items: %d  (%d Sel)", GlobalDir.Count, Selected);
                
                DrawTextStr(&GlobalRenderer, &Font, 10 * GlobalScale, StatusY + (4 * GlobalScale), StatusIdx, MutedColor);
                DrawTextStr(&GlobalRenderer, &Font, 10 * GlobalScale, StatusY + (16 * GlobalScale), StatusIms, (f32[]){0.6f, 0.6f, 0.6f, 1});

                // --- App Face ---
                f32 FaceX = Width - 36 * GlobalScale;
                f32 FaceY = StatusY + 7 * GlobalScale;
                Render8BitIcon(ICON_APP_FACE, FaceX, FaceY, GlobalScale);

                // --- ABSOLUTE FINAL DIAGNOSTICS ---
                // REMOVED FOR CLEAN BUILD

                // --- STATUS BAR (Path Display) ---
                // StatusH/StatusY defined above
                
                // Background for Status Bar
                RenderQuad(&GlobalRenderer, SidebarWidth + 1, StatusY, ListWidth, StatusH, (f32[]){0.12f, 0.12f, 0.12f, 1.0f});
                RenderQuad(&GlobalRenderer, SidebarWidth + 1, StatusY, ListWidth, 1.0f, (f32[]){0.0f, 0.0f, 0.0f, 0.3f}); // Sep Line
                
                char PathFull[MAX_PATH];
                wsprintfA(PathFull, "%S", GlobalCurrentPath);
                DrawTextStr(&GlobalRenderer, &Font, SidebarWidth + 16*GlobalScale, StatusY + 6*GlobalScale, PathFull, (f32[]){0.7f, 0.7f, 0.7f, 1.0f});

                EndFrame(&GlobalRenderer);
                
                GlobalLeftClickOneShot = false; // Reset Click Flag
            } // End While
        } // End If (Window)
    } // End If (RegisterClass)
    
    CoUninitialize();
    return 0;
}

LRESULT CALLBACK
Win32WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
            if(GlobalRenderer.Device)
            {
                f32 Width = (f32)LOWORD(LParam);
                f32 Height = (f32)HIWORD(LParam);
                ResizeD3D11(&GlobalRenderer, Width, Height);
            }
        } break;

        case WM_CLOSE:
        {
            GlobalRunning = false;
        } break;

        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;

        case WM_CHAR:
        {
            char C = (char)WParam;
            
            // Search Trigger (Enter)
            if(C == 13 && GlobalSearchFocused)
            {
                 // Trigger Global Search
                 wchar_t WQuery[256];
                 wsprintfW(WQuery, L"%S", GlobalSearchBuffer);
                 
                 directory_list Results = QueryIndex(&GlobalPartition, WQuery);
                 GlobalDir = Results; // Replace current view with results
                 GlobalCurrentPath[0] = 0; // Set to "Search Results"
                 wsprintfW(GlobalCurrentPath, L"Search: %S", GlobalSearchBuffer);
                 GlobalScrollY = 0;
                 GlobalTargetScrollY = 0;
                 GlobalSelectedIndex = -1;
                 GlobalSearchFocused = false; // Unfocus after enter
                 break;
            }

            // --- NUCLEAR FALLBACK KEYBOARD SHORTCUTS ---
            if(!GlobalSearchFocused)
            {
                if(C == 's' || C == 'S')
                {
                    GlobalViewMode = 1;
                    GlobalScrollY = 0;
                    GlobalTargetScrollY = 0;
                    if(!GlobalStorageTree && !GlobalIsAnalyzing)
                    {
                        GlobalIsAnalyzing = true;
                        wsprintfW(GlobalAnalysisPath, L"%s", GlobalCurrentPath);
                        CreateThread(0, 0, StorageAnalysisThread, 0, 0, 0);
                    }
                }
                else if(C == 27) // Escape
                {
                    GlobalViewMode = 0;
                }
            }
            
            char *TargetBuffer = 0;
            int MaxLen = 254;
            
            if(GlobalContextMenuActive) { TargetBuffer = GlobalMenuSearch; MaxLen = 63; }
            else if(GlobalSearchFocused) { TargetBuffer = GlobalSearchBuffer; }
            
            if(TargetBuffer)
            {
                if(C == 8) // Backspace
                {
                    int Len = lstrlenA(TargetBuffer);
                    if(Len > 0) TargetBuffer[Len-1] = 0;
                }
                else if(C >= 32 && C <= 126)
                {
                    int Len = lstrlenA(TargetBuffer);
                    if(Len < MaxLen)
                    {
                        TargetBuffer[Len] = C;
                        TargetBuffer[Len+1] = 0;
                    }
                }
            }
        } break;

        case WM_KEYDOWN:
        {
            if(WParam == VK_ESCAPE)
            {
                GlobalSearchFocused = false;
                GlobalContextMenuActive = false;
            }
        } break;



        case WM_MOUSEWHEEL:
        {
            i16 WheelDelta;
            RECT ClientRect_Anim;
            f32 Width_Anim, SidebarWidth, PreviewWidth, ListWidth, PreviewX;
            
            WheelDelta = (i16)HIWORD(WParam);
            GetClientRect(Window, &ClientRect_Anim);
            Width_Anim = (f32)(ClientRect_Anim.right - ClientRect_Anim.left);
            SidebarWidth = 220.0f * GlobalScale;
            PreviewWidth = Width_Anim * 0.3f;
            if(PreviewWidth < 300 * GlobalScale) PreviewWidth = 300 * GlobalScale;
            ListWidth = Width_Anim - SidebarWidth - PreviewWidth;
            PreviewX = SidebarWidth + ListWidth;
            
            if(GlobalMouseX > PreviewX)
            {
                GlobalPreviewScrollY -= (f32)WheelDelta;
                if(GlobalPreviewScrollY < 0) GlobalPreviewScrollY = 0;
            }
            else
            {
                GlobalTargetScrollY -= (f32)WheelDelta; 
                if(GlobalTargetScrollY < 0) GlobalTargetScrollY = 0;
            }
        } break;
        
        case WM_MOUSEMOVE:
        {
            GlobalMouseX = (short)LOWORD(LParam);
            GlobalMouseY = (short)HIWORD(LParam);
            
            if(GlobalIsDragging)
            {
                GlobalDragEndX = GlobalMouseX;
                GlobalDragEndY = GlobalMouseY;
            }
        } break;

        case WM_LBUTTONUP:
        {
            if(GlobalIsDragging)
            {
                GlobalIsDragging = false;
                // Commit Selection logic here if we were doing fancy multi-select
                // For now, the visual box is just visual
            }
        } break;

        case WM_LBUTTONDOWN:
        {
            int MouseX = (short)LOWORD(LParam);
            int MouseY = (short)HIWORD(LParam);
            
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            f32 Width = (f32)(ClientRect.right - ClientRect.left);
            
            f32 SidebarWidth = 220.0f * GlobalScale;
            f32 PreviewWidth = Width * 0.3f;
            if(PreviewWidth < 300 * GlobalScale) PreviewWidth = 300 * GlobalScale;
            f32 ListWidth = Width - SidebarWidth - PreviewWidth;
            GlobalLeftClickOneShot = true;
            
            f32 HeaderHeight = 54.0f * GlobalScale;
            if(GlobalViewMode == 1 && !GlobalContextMenuActive)
            {
                 // Storage View only intercepts clicks BELOW the header
                 if(MouseY > HeaderHeight) return 0;
            }

            if(GlobalContextMenuActive)
            {
                // Re-Simulate Filter to find what was clicked
                // MUST MATCH RENDER LOGIC
                menu_item AllItems[] = { {"Open", 0}, {"Copy Path", 1}, {"Properties", 2}, {"Calc Folder Size", 3}, 
                                         {"Delete", 4}, {"Reveal in Explorer", 5}, {"Open Terminal", 6} };
                int MatchCount = 0;
                int Matches[10];
                for(int m=0; m<7; ++m) {
                    if(!GlobalMenuSearch[0] || CaseInsensitiveContains(AllItems[m].Name, GlobalMenuSearch))
                    {
                         if(MatchCount < 10) Matches[MatchCount++] = m;
                    }
                }
                
                f32 MenuW = 200.0f * GlobalScale;
                f32 HeaderH = 35.0f * GlobalScale;
                f32 ItemH = 24.0f * GlobalScale;

                // Hit Test (Scale-Aware)
                if(MouseX >= GlobalContextMenuX && MouseX <= GlobalContextMenuX + MenuW &&
                   MouseY >= GlobalContextMenuY + HeaderH && MouseY <= GlobalContextMenuY + HeaderH + (MatchCount * ItemH))
                {
                    int RelY = (int)(MouseY - (GlobalContextMenuY + HeaderH));
                    int ClickedIndex = (int)(RelY / ItemH);
                    
                    if(ClickedIndex >= 0 && ClickedIndex < MatchCount)
                    {
                        int ActionId = AllItems[Matches[ClickedIndex]].Id;
                        
                        // DRIVE ACTIONS
                        if(GlobalContextMenuIndex == -2)
                        {
                            if(ActionId == 3) // Analyze
                            {
                                LogMessage("Action: Calc Folder Size for Drive");
                                GlobalStorageTree = AnalyzeStorage(&GlobalPreviewArena, GlobalSelectedDrive, 0);
                                GlobalInspectorTab = 1; // 1=Storage
                            }
                            else if(ActionId == 0) // Open
                            {
                                ChangeDirectory(GlobalSelectedDrive, true);
                            }
                            GlobalContextMenuActive = false;
                            return 0;
                        }

                        // FILE ACTIONS
                        file_info *File = 0;
                        if(GlobalContextMenuIndex >= 0) File = GlobalDir.Files + GlobalContextMenuIndex;
                        
                        if(ActionId == 0 && File) // Open
                        {
                            if(File->IsDirectory)
                            {
                                 wchar_t NewPath[MAX_PATH];
                                 int P_Len = lstrlenW(GlobalCurrentPath);
                                 if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(NewPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                                 else wsprintfW(NewPath, L"%s%s", GlobalCurrentPath, File->FileName);
                                 ChangeDirectory(NewPath, true);
                            }
                            else
                            {
                                 wchar_t FullPath[MAX_PATH];
                                 int P_Len = lstrlenW(GlobalCurrentPath);
                                 if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(FullPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                                 else wsprintfW(FullPath, L"%s%s", GlobalCurrentPath, File->FileName);
                                 ShellExecuteW(0, L"open", FullPath, 0, 0, SW_SHOW);
                            }
                        }
                        else if(ActionId == 1 && File) // Copy Path
                        {
                            if(OpenClipboard(Window))
                            {
                                EmptyClipboard();
                                wchar_t Path[MAX_PATH];
                                int P_Len = lstrlenW(GlobalCurrentPath);
                                if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(Path, L"%s\\%s", GlobalCurrentPath, File->FileName);
                                else wsprintfW(Path, L"%s%s", GlobalCurrentPath, File->FileName);
                                
                                int Len = lstrlenW(Path);
                                HGLOBAL Mem = GlobalAlloc(GMEM_MOVEABLE, (Len + 1) * sizeof(wchar_t));
                                wchar_t *Dest = (wchar_t *)GlobalLock(Mem);
                                for(int i=0; i<=Len; ++i) Dest[i] = Path[i];
                                GlobalUnlock(Mem);
                                SetClipboardData(CF_UNICODETEXT, Mem);
                                CloseClipboard();
                            }
                        }
                        else if(ActionId == 2 && File) // Properties
                        {
                             wchar_t Path[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(Path, L"%s\\%s", GlobalCurrentPath, File->FileName);
                             else wsprintfW(Path, L"%s%s", GlobalCurrentPath, File->FileName);

                             SHELLEXECUTEINFOW Sei = {0};
                             Sei.cbSize = sizeof(Sei);
                             Sei.fMask = SEE_MASK_INVOKEIDLIST;
                             Sei.lpVerb = L"properties";
                             Sei.lpFile = Path;
                             Sei.nShow = SW_SHOW;
                             ShellExecuteExW(&Sei);
                             
                             GlobalContextMenuActive = false; // Close menu
                        }
                         else if(ActionId == 3 && File) // Calc Folder Size
                         {
                             GlobalSelectedIndex = GlobalContextMenuIndex;
                             if(File->IsDirectory)
                             {
                                  wchar_t Path[MAX_PATH];
                                  int P_Len = lstrlenW(GlobalCurrentPath);
                                  if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(Path, L"%s\\%s", GlobalCurrentPath, File->FileName);
                                  else wsprintfW(Path, L"%s%s", GlobalCurrentPath, File->FileName);
                                  
                                  // Async Queue Job
                                  wchar_t *JobPath = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lstrlenW(Path)+1)*sizeof(wchar_t));
                                  lstrcpyW(JobPath, Path);
                                  extern void CalculateFolderSizeJob(void *Data);
                                  AddWork(&GlobalWorkQueue, CalculateFolderSizeJob, JobPath);
                                  
                                  // Trigger Visualization
                                  GlobalViewMode = 1; 
                                  GlobalInspectorTab = 1;
                                  
                                  extern memory_arena GlobalStorageArena;
                                  GlobalStorageArena.Used = 0;
                                  GlobalStorageTree = AnalyzeStorage(&GlobalStorageArena, Path, 0);
                             }
                         }
                        else if(ActionId == 4 && File) // Delete
                        {
                             wchar_t Path[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(Path, L"%s\\%s", GlobalCurrentPath, File->FileName);
                             else wsprintfW(Path, L"%s%s", GlobalCurrentPath, File->FileName);
                             
                             // Simple Delete (No Undo! Dangerous but requested)
                             if(File->IsDirectory) RemoveDirectoryW(Path); 
                             else DeleteFileW(Path);
                             
                             // Refresh
                             ChangeDirectory(GlobalCurrentPath, false);
                        }
                        else if(ActionId == 5 && File) // Reveal in Explorer
                        {
                             wchar_t Path[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(Path, L"%s\\%s", GlobalCurrentPath, File->FileName);
                             else wsprintfW(Path, L"%s%s", GlobalCurrentPath, File->FileName);
                             
                             wchar_t Args[MAX_PATH + 50];
                             // Robust Quoting for Path with Spaces
                             wsprintfW(Args, L"/select,\"%s\"", Path);
                             
                             SHELLEXECUTEINFOW Sei = {0};
                             Sei.cbSize = sizeof(Sei);
                             Sei.fMask = SEE_MASK_DEFAULT;
                             Sei.lpFile = L"explorer.exe"; // Force System Explorer without hardcoded C drive assumptions
                             Sei.lpParameters = Args;
                             Sei.nShow = SW_SHOWNORMAL;
                             ShellExecuteExW(&Sei);
                        }
                        else if(ActionId == 6 && File) // Open Terminal
                        {
                             wchar_t Path[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(Path, L"%s\\%s", GlobalCurrentPath, File->FileName);
                             else wsprintfW(Path, L"%s%s", GlobalCurrentPath, File->FileName);
                             
                             wchar_t TermPath[MAX_PATH];
                             if(!File->IsDirectory) {
                                 // For files, use Parent Dir
                                 lstrcpyW(TermPath, GlobalCurrentPath);
                             } else {
                                 lstrcpyW(TermPath, Path);
                             }
                             
                             wchar_t Args[MAX_PATH + 50];
                             wsprintfW(Args, L"/k cd /d \"%s\"", TermPath);
                             ShellExecuteW(0, 0, L"cmd.exe", Args, 0, SW_SHOW);
                        }
                        GlobalContextMenuActive = false;
                        return 0;
                    }
                }
                
                // Click outside menu dims it? No, just close.
                // NOTE: If clicking in Search Bar (Top of menu), we should keep it open!
                if(MouseX >= GlobalContextMenuX && MouseX <= GlobalContextMenuX + MenuW &&
                   MouseY >= GlobalContextMenuY && MouseY <= GlobalContextMenuY + HeaderH)
                {
                    // Clicked Menu Header/Search. keep open.
                    return 0;
                }
                
                GlobalContextMenuActive = false;
            }

            // --- Unified Hit-Testing Logic ---
            // If any of the "Hot" flags from the last frame were set during a click, 
            // the render loop logic will have already handled the transition.
            // We only need to check for things that aren't calculated in the immediate tool-strip.
            
            // Unfocus Search if clicking elsewhere
            if(GlobalSearchFocused && !GlobalSearchHot)
            {
                 // We need GlobalSearchHot from the render loop. Let's make it more robust.
            }

            // --- Hit Test ---
            SidebarWidth = 220.0f * GlobalScale;
            if(MouseX < SidebarWidth)
            {
                // Sidebar Click
                f32 SideY = HeaderHeight + (20.0f + 32.0f) * GlobalScale; // Start of Pinned Items (Header + Spacing)
                f32 ItemH = 30.0f * GlobalScale;
                
                // Item Height 28
                int CSIDLs[] = {CSIDL_DESKTOPDIRECTORY, CSIDL_PERSONAL, -1, CSIDL_MYMUSIC, CSIDL_MYPICTURES, CSIDL_MYVIDEO};
                
                // Check Pinned (6 items)
                for(int i=0; i<6; ++i)
                {
                     if(MouseY >= SideY && MouseY < SideY + ItemH)
                     {
                         wchar_t Path[MAX_PATH];
                         b32 Found = false;
                         
                         if(CSIDLs[i] == -1) // Downloads (Not a standard CSIDL, usually local)
                         {
                             wchar_t *UserProfile = _wgetenv(L"USERPROFILE");
                             if(UserProfile) {
                                  wsprintfW(Path, L"%s\\Downloads", UserProfile);
                                  Found = true;
                             }
                         }
                         else if(CSIDLs[i] == CSIDL_DESKTOPDIRECTORY) // Explicit Desktop override for OneDrive
                         {
                             wchar_t *UserProfile = _wgetenv(L"USERPROFILE");
                             if(UserProfile) {
                                  wsprintfW(Path, L"%s\\OneDrive\\Desktop", UserProfile);
                                  if(GetFileAttributesW(Path) == INVALID_FILE_ATTRIBUTES) {
                                      wsprintfW(Path, L"%s\\Desktop", UserProfile); // Fallback
                                  }
                                  Found = true;
                             }
                         }
                         else
                         {
                             if(SUCCEEDED(SHGetFolderPathW(0, CSIDLs[i], 0, SHGFP_TYPE_CURRENT, Path))) Found = true;
                         }

                         if(Found) ChangeDirectory(Path, true);
                         return 0;
                     }
                     SideY += ItemH;
                }
                
                SideY += (24.0f + 32.0f) * GlobalScale; // Spacing + Drives Header Spacing
                
                DWORD Drives = GetLogicalDrives();
                for(char Letter='A'; Letter<='Z'; Letter++)
                {
                    if(Drives & 1)
                    {
                        if(MouseY >= SideY && MouseY < SideY + ItemH)
                        {
                            wchar_t DrivePath[10];
                            wsprintfW(DrivePath, L"%c:\\", Letter);
                            ChangeDirectory(DrivePath, true);
                            return 0;
                        }
                        SideY += ItemH;
                    }
                    Drives >>= 1;
                }
            }
            else if(MouseX < SidebarWidth + ListWidth)
            {
                // List Click
                if(GlobalViewMode == 1) return 0; // Handled by RenderStorageView (Immediate Mode)
                
                f32 ListTop = GlobalListTop; 
                if(MouseY < ListTop)
                {
                    // HEADER CLICKS:
                    // Handled entirely by Render Loop (deferred) via GlobalLeftClickOneShot.
                    // This ensures the Hit Box matches the drawn UI exactly (scaled coords).
                }
                else if(MouseY >= ListTop)
                {
                    int Index = (int)((MouseY - ListTop + GlobalScrollY) / GlobalRowHeight); 
                    GlobalSelectedIndex = Index;
                    GlobalPreviewLoaded = false;
                    
                    // Start Drag
                    GlobalIsDragging = true;
                    GlobalDragStartX = MouseX;
                    GlobalDragStartY = MouseY;
                    GlobalDragEndX = MouseX;
                    GlobalDragEndY = MouseY;
                }
            }
            else
            {
                 // Preview Pane Click
                 f32 PreviewX = SidebarWidth + ListWidth;
                 f32 PreviewY = 32.0f + 10.0f; // Tab Bar + Padding
                 
                 f32 ContentX = PreviewX + UI_GUTTER * GlobalScale;
                 f32 ContentY = PreviewY;

                 // "OPEN" Button Hit Test
                 int BtnX = (int)ContentX;
                 int BtnY = (int)ContentY;
                 int BtnW = (int)(90 * GlobalScale);
                 int BtnH = (int)(30 * GlobalScale);

                 if(MouseX >= BtnX && MouseX <= BtnX + BtnW &&
                    MouseY >= BtnY && MouseY <= BtnY + BtnH)
                 {
                     if(GlobalSelectedIndex >= 0 && GlobalSelectedIndex < (int)GlobalDir.Count)
                     {
                         file_info *File = GlobalDir.Files + GlobalSelectedIndex;
                         wchar_t FullPath[MAX_PATH];
                         int P_Len = lstrlenW(GlobalCurrentPath);
                         if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(FullPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                         else wsprintfW(FullPath, L"%s%s", GlobalCurrentPath, File->FileName);
                         
                         if(File->IsDirectory) ChangeDirectory(FullPath, true);
                         else ShellExecuteW(0, L"open", FullPath, 0, 0, SW_SHOW);
                     }
                     return 0;
                 }
                 
                 // Content List Hit Test
                 if(GlobalPreviewDir.Count > 0)
                 {
                     f32 ListStartY = ContentY + (45.0f * GlobalScale) + (25.0f * GlobalScale); // Matches inspector.c progression
                     if(MouseY >= ListStartY)
                     {
                         int Index = (int)((MouseY - ListStartY) / (24 * GlobalScale));
                         if(Index >= 0 && Index < (int)GlobalPreviewDir.Count)
                         {
                             file_info *Target = GlobalPreviewDir.Files + Index;
                             file_info *Parent = GlobalDir.Files + GlobalSelectedIndex;
                             
                             wchar_t ParentPath[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(ParentPath, L"%s\\%s", GlobalCurrentPath, Parent->FileName);
                             else wsprintfW(ParentPath, L"%s%s", GlobalCurrentPath, Parent->FileName);
                             
                             wchar_t NewPath[MAX_PATH];
                             wsprintfW(NewPath, L"%s\\%s", ParentPath, Target->FileName);
                             
                             if(Target->IsDirectory) ChangeDirectory(NewPath, true);
                             else {
                                 ChangeDirectory(ParentPath, true);
                                 for(int k=0; k<(int)GlobalDir.Count; ++k) {
                                     if(lstrcmpiW(GlobalDir.Files[k].FileName, Target->FileName) == 0) {
                                         GlobalSelectedIndex = k;
                                         GlobalTargetScrollY = (f32)(k * GlobalRowHeight) - 100.0f;
                                         if(GlobalTargetScrollY < 0) GlobalTargetScrollY = 0;
                                         GlobalPreviewLoaded = false;
                                         break;
                                     }
                                 }
                             }
                         }
                     }
                 }
            }

        } break;
        
        case WM_LBUTTONDBLCLK:
        {
            // Handle Double Click on List
            int MouseX = (short)LOWORD(LParam);
            int MouseY = (short)HIWORD(LParam);
            
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            f32 Width = (f32)(ClientRect.right - ClientRect.left);
            
            f32 SidebarWidth = 220.0f * GlobalScale;
            f32 PreviewWidth = Width * 0.3f;
            if(PreviewWidth < 300 * GlobalScale) PreviewWidth = 300 * GlobalScale;
            f32 ListWidth = Width - SidebarWidth - PreviewWidth;
            
            if(MouseX >= SidebarWidth && MouseX < SidebarWidth + ListWidth)
            {
                 f32 ListTop = GlobalListTop; 
                 if(MouseY >= ListTop)
                 {
                    int Index = (int)((MouseY - ListTop + GlobalScrollY) / GlobalRowHeight); 
                    if(Index == GlobalSelectedIndex && Index >= 0 && Index < (int)GlobalDir.Count)
                    {
                         // Double Click on Item
                         file_info *File = GlobalDir.Files + Index;
                         if(File->IsDirectory)
                         {
                             wchar_t NewPath[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(NewPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                             else wsprintfW(NewPath, L"%s%s", GlobalCurrentPath, File->FileName);
                             ChangeDirectory(NewPath, true);
                         }
                         else
                         {
                             wchar_t FullPath[MAX_PATH];
                             int P_Len = lstrlenW(GlobalCurrentPath);
                             if(GlobalCurrentPath[P_Len-1] != '\\') wsprintfW(FullPath, L"%s\\%s", GlobalCurrentPath, File->FileName);
                             else wsprintfW(FullPath, L"%s%s", GlobalCurrentPath, File->FileName);
                             ShellExecuteW(0, L"open", FullPath, 0, 0, SW_SHOW);
                         }
                    }
                 }
            }
        } break;
        
        case WM_RBUTTONDOWN:
        {
            int MouseX = (short)LOWORD(LParam);
            int MouseY = (short)HIWORD(LParam);
            
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            f32 Width = (f32)(ClientRect.right - ClientRect.left);
            f32 SidebarWidth = 220.0f * GlobalScale;
            f32 PreviewWidth = Width * 0.3f;
            if(PreviewWidth < 300 * GlobalScale) PreviewWidth = 300 * GlobalScale;
            f32 ListWidth = Width - SidebarWidth - PreviewWidth;

            if(MouseX >= SidebarWidth && MouseX < SidebarWidth + ListWidth)
            {
                f32 ListTop = GlobalListTop;
                if(MouseY >= ListTop)
                {
                     int Index = (int)((MouseY - ListTop + GlobalScrollY) / GlobalRowHeight);
                     if(Index >= 0 && Index < (int)GlobalDir.Count)
                     {
                        GlobalSelectedIndex = Index;
                        GlobalPreviewLoaded = false;
                        
                        GlobalContextMenuActive = true;
                        GlobalContextMenuX = MouseX;
                        GlobalContextMenuY = MouseY;
                        GlobalContextMenuIndex = Index;
                     }
                }
            }
            else if(MouseX < SidebarWidth) // Sidebar Click
            {
                 // Replicate Layout Logic (Same as WM_LBUTTONDOWN)
                 f32 HeaderHeight = 54.0f * GlobalScale;
                 f32 SideY = HeaderHeight + (20.0f + 32.0f) * GlobalScale;
                 f32 ItemH = 30.0f * GlobalScale;
                 
                 // Pinned (skip for right click for now or just match)
                 SideY += 6 * ItemH; 
                 SideY += (24.0f + 32.0f) * GlobalScale; 
                 
                 DWORD Drives = GetLogicalDrives();
                 for(char Letter='A'; Letter<='Z'; Letter++)
                 {
                     if(Drives & 1)
                     {
                         if(MouseY >= SideY && MouseY < SideY + ItemH)
                         {
                             // Right Clicked a Drive!
                             GlobalContextMenuActive = true;
                             GlobalContextMenuX = MouseX;
                             GlobalContextMenuY = MouseY;
                             GlobalContextMenuIndex = -2; // Special ID for Drive
                             
                             wsprintfW(GlobalSelectedDrive, L"%c:\\", Letter);
                             GlobalMenuSearch[0] = 0; 
                             break;
                         }
                         SideY += ItemH;
                     }
                     Drives >>= 1;
                 }
            }
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            BeginPaint(Window, &Paint);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProcW(Window, Message, WParam, LParam);
        } break;
    }

    // Shutdown
    SaveCache();
    return Result;
}


// Unity Build Includes
#include "animation.c"
#include "ui_icons.c"
#include "ui_components.c"
#include "multithreading.c"
#include "renderer.c"
#include "font.c"
#include "storage_cache.c"
#include "filesystem.c"
#include "search_engine.c"
#include "render_storage.c"
#include "render_home.c"
#include "inspector.c"

// --- App Icon Implementation (Standard Embedded Resource) ---
void SetAppIcon(HWND Window)
{
    // Load Icon from Executable Resource (ID 1)
    // Now that we have a valid ICO resource, standard API works perfect.
    HICON hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(1));
    
    if(hIcon)
    {
        SendMessage(Window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(Window, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SetClassLongPtrW(Window, GCLP_HICON, (LONG_PTR)hIcon);
        SetClassLongPtrW(Window, GCLP_HICONSM, (LONG_PTR)hIcon);
    }
}
