#include "inspector.h"
#include <stdio.h>

// Helper to sort Storage Nodes for display (descending size)
int CompareStorageDesc(const void *A, const void *B) {
    storage_node **NA = (storage_node**)A;
    storage_node **NB = (storage_node**)B;
    if((*NA)->Size < (*NB)->Size) return 1;
    if((*NA)->Size > (*NB)->Size) return -1;
    return 0;
}

// External Global Access (Unity Build)
extern int GlobalInspectorTab;
extern f32 GlobalScale;
extern memory_arena GlobalPreviewArena;
extern wchar_t GlobalSelectedDrive[16];
extern ID3D11ShaderResourceView *GlobalPreviewTexture;
extern b32 GlobalPreviewLoaded;
extern char GlobalPreviewBuffer[]; // 64KB Buffer
extern directory_list GlobalDir;
extern int GlobalSelectedIndex;
extern f32 GlobalPreviewScrollY;

void Inspector_Draw(renderer_state *Renderer, font_atlas *Font, f32 X, f32 Y, f32 W, f32 H,
                    int MouseX, int MouseY, f32 *TextColor, f32 *MutedColor, f32 *SepColor,
                    directory_list *PreviewList)
{
    f32 CurrentY = Y;
    
    // --- Tab Bar ---
    f32 TabH = 32.0f * GlobalScale;
    f32 TabW = W;
    
    // Tab Backgrounds
    f32 BgActive[] = {0.15f, 0.15f, 0.15f, 1.0f}; 
    
    // No more interactive tabs, just static Preview header
    RenderQuad(Renderer, X, Y, TabW, TabH, BgActive);
    DrawTextStr(Renderer, Font, X + 15*GlobalScale, Y + 8*GlobalScale, "Preview", TextColor);
    
    CurrentY += TabH;
    
    // Handle Click state globally for all interactive components
    static b32 MouseDownLast = false; 
    b32 MouseDown = (GetKeyState(VK_LBUTTON) & 0x8000) != 0;
    b32 Clicked = MouseDown && !MouseDownLast;
    MouseDownLast = MouseDown;
    
    // Border
    RenderQuad(Renderer, X, CurrentY, W, 1, SepColor);
    CurrentY += 10.0f; // Padding

    f32 ContentX = X + UI_GUTTER;
    f32 ContentW = W - (UI_GUTTER * 2);

    // --- Content Switch ---
    if(GlobalInspectorTab == 1)
    {
         // STORAGE MODE
         if(!GlobalStorageTree)
         {
             DrawTextStr(Renderer, Font, ContentX, CurrentY, "No Analysis Data.", MutedColor); CurrentY += 24;
             DrawTextStr(Renderer, Font, ContentX, CurrentY, "Click 'Open' or use", MutedColor); CurrentY += 24;
             DrawTextStr(Renderer, Font, ContentX, CurrentY, "Context Menu -> Analyze", MutedColor); CurrentY += 35;
             
             // Analyze Button
             f32 BtnColor[] = {0.2f, 0.4f, 0.6f, 1.0f};
             if(MouseX >= ContentX && MouseX <= ContentX+120 && MouseY >= CurrentY && MouseY <= CurrentY+30 && Clicked)
             {
                 GlobalStorageTree = AnalyzeStorage(&GlobalPreviewArena, GlobalCurrentPath, 0);
             }
             RenderRoundedQuad(Renderer, ContentX, CurrentY, 120, 30, 4.0f, BtnColor);
             DrawTextStr(Renderer, Font, ContentX+10, CurrentY+6, "Analyze Current", TextColor);
         }
         else
         {
            // Root Info
            char RootBuf[128];
            f32 RootMB = (f32)GlobalStorageTree->Size / (1024*1024);
            sprintf_s(RootBuf, sizeof(RootBuf), "Total: %.1f MB", RootMB);
            DrawTextStr(Renderer, Font, ContentX, CurrentY, RootBuf, TextColor);
            CurrentY += 30.0f;
            
            // Collect Children for Sorting
            if(GlobalStorageTree->ChildCount > 0)
            {
                 // Temp Array on Stack 
                 storage_node *Nodes[256];
                 int Count = 0;
                 storage_node *Child = GlobalStorageTree->FirstChild;
                 while(Child && Count < 256) {
                     Nodes[Count++] = Child;
                     Child = Child->NextSibling;
                 }
                 
                 // Sort
                 qsort(Nodes, Count, sizeof(storage_node*), CompareStorageDesc);
                 
                 // Render Top Items
                 for(int i=0; i<Count && i<20; ++i)
                 {
                     storage_node *Node = Nodes[i];
                     f32 Pct = (f32)Node->Size / (f32)GlobalStorageTree->Size;
                     if(Pct < 0.01f && i > 5) break; 
                     
                     // Bar Background
                     RenderRoundedQuad(Renderer, ContentX, CurrentY, ContentW, 24, 4.0f, (f32[]){0.2f,0.2f,0.2f,1});
                     // Usage Bar
                     f32 BarW = ContentW * Pct;
                     if(BarW < 4) BarW = 4;
                     
                     f32 BarColor[] = {0.3f, 0.5f, 0.7f, 1.0f};
                     if(Node->IsDirectory) { BarColor[0]=0.8f; BarColor[1]=0.6f; BarColor[2]=0.2f; }
                     
                     RenderRoundedQuad(Renderer, ContentX, CurrentY, BarW, 24, 4.0f, BarColor);
                     
                     // Name Overlay
                     char NameBuf[128];
                     f32 SizeMB = (f32)Node->Size / (1024*1024);
                     
                     char AnsiName[64];
                     for(int k=0; k<63; ++k) { AnsiName[k] = (char)Node->Name[k]; if(!Node->Name[k]) break; }
                     AnsiName[63] = 0;
    
                     sprintf_s(NameBuf, sizeof(NameBuf), "%s (%.1f MB)", AnsiName, SizeMB);
                     DrawTextStr(Renderer, Font, ContentX + 8, CurrentY+4, NameBuf, (f32[]){1,1,1,1});
                     
                     CurrentY += 28.0f;
                 }
            }
         }
         return; 
    }

    // PREVIEW MODE (Tab 0) - Existing Logic
    
    // OPEN Button (Styled)
    f32 ButtonColor[] = {0.2f, 0.6f, 0.3f, 1.0f}; // Nice Green
    f32 BtnHover[]    = {0.3f, 0.7f, 0.4f, 1.0f};
    
    extern f32 GlobalScale;
    
    // Mouse Check (Visual Only)
    b32 BtnHot = (MouseX >= ContentX && MouseX <= ContentX + 90 * GlobalScale &&
                  MouseY >= CurrentY && MouseY <= CurrentY + 30 * GlobalScale);
    
    RenderRoundedQuad(Renderer, ContentX, CurrentY, 90 * GlobalScale, 30 * GlobalScale, ICON_RADIUS + 1.0f, BtnHot ? BtnHover : ButtonColor);
    DrawTextStr(Renderer, Font, ContentX + 20 * GlobalScale, CurrentY + 6 * GlobalScale, "OPEN", (float[]){1,1,1,1});
    CurrentY += 45.0f * GlobalScale;

    // Content
    if(GlobalPreviewTexture)
    {
        // Get Dimensions
        ID3D11Resource *Res = 0;
        GlobalPreviewTexture->lpVtbl->GetResource(GlobalPreviewTexture, &Res);
        if(Res)
        {
            ID3D11Texture2D *Tex = (ID3D11Texture2D*)Res;
            D3D11_TEXTURE2D_DESC Desc;
            Tex->lpVtbl->GetDesc(Tex, &Desc);
            Res->lpVtbl->Release(Res);
            
            f32 Aspect = (f32)Desc.Width / (f32)Desc.Height;
            f32 DrawW = ContentW;
            f32 DrawH = DrawW / Aspect;
            
            // Limit Height
            f32 MaxH = (Y + H) - CurrentY - 40;
            if(DrawH > MaxH) {
                DrawH = MaxH;
                DrawW = DrawH * Aspect;
            }
            
            SetTexture(Renderer, GlobalPreviewTexture);
            RenderQuadUV(Renderer, ContentX, CurrentY, DrawW, DrawH, 0, 0, 1, 1, (float[]){1,1,1,1});
            CurrentY += DrawH + 10;
            
            char Info[64];
            sprintf_s(Info, sizeof(Info), "%dx%d  Image", Desc.Width, Desc.Height);
            DrawTextStr(Renderer, Font, ContentX, CurrentY, Info, MutedColor);
        }
    }
    else if(PreviewList && GlobalSelectedIndex >= 0 && GlobalSelectedIndex < (int)GlobalDir.Count && GlobalDir.Files[GlobalSelectedIndex].IsDirectory)
    {
        // Render Directory List
        char TitleBuf[512] = "Folder Contents:";
        if(GlobalSelectedIndex >= 0 && GlobalSelectedIndex < (int)GlobalDir.Count) {
            wsprintfA(TitleBuf, "Contents of %S:", GlobalDir.Files[GlobalSelectedIndex].FileName);
        }
        DrawTextStr(Renderer, Font, ContentX, CurrentY, TitleBuf, MutedColor); CurrentY += 25;

        if(PreviewList->Count == 0) {
            DrawTextStr(Renderer, Font, ContentX, CurrentY, "(Empty Folder)", MutedColor);
        }

        extern f32 GlobalScale;
        int SkipRows = (int)(GlobalPreviewScrollY / (24.0f*GlobalScale));
        if(SkipRows < 0) SkipRows = 0;

        for(int i=SkipRows; i<(int)PreviewList->Count; ++i)
        {
            file_info *File = PreviewList->Files + i;
            f32 RowH = 24.0f * GlobalScale;

            // Hover Check (Simple - Visual Only)
            if(MouseX >= X && MouseX < X+W && MouseY >= CurrentY && MouseY < CurrentY+RowH)
            {
                 f32 ItemHover[] = {0.22f, 0.22f, 0.23f, 1.0f};
                 RenderRoundedQuad(Renderer, X + (UI_GUTTER/2), CurrentY, W - UI_GUTTER, RowH, ICON_RADIUS, ItemHover);
            }

            // 8-Bit Icon
            icon_type IType = DetermineIconType(File->FileName, File->IsDirectory);
            Render8BitIcon(IType, ContentX, CurrentY+4*GlobalScale, GlobalScale * 0.8f);

            char NameBuf[256];
            wsprintfA(NameBuf, "%S", File->FileName);
            DrawTextStr(Renderer, Font, ContentX + 24*GlobalScale, CurrentY+4*GlobalScale, NameBuf, TextColor);

            CurrentY += RowH;
            if(CurrentY > Y + H) break;
        }
    }
    else
    {
        // Text Content
        char *Lines = GlobalPreviewBuffer;
        int SkipLines = (int)(GlobalPreviewScrollY / 20.0f);
        if(SkipLines < 0) SkipLines = 0;
        int LineCount = 0;
        
        while(*Lines)
        {
             char *NextLine = Lines;
             while(*NextLine && *NextLine != '\n') NextLine++;
             char Old = *NextLine;
             *NextLine = 0;
             
             if(LineCount >= SkipLines) {
                 DrawTextStr(Renderer, Font, ContentX, CurrentY, Lines, TextColor);
                 CurrentY += 20; 
             }
             
             *NextLine = Old;
             if(!Old) break;
             Lines = NextLine + 1;
             LineCount++;
             if(CurrentY > Y + H) break; 
        }
    }
}
