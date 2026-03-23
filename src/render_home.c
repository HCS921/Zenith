#include "renderer.h"
#include "font.h"
#include <windows.h>
#include <stdio.h>

extern renderer_state GlobalRenderer;
extern font_atlas Font;
extern f32 GlobalRowHeight;

// Helper from win32_main or copy? Check if ChangeDirectory is available.
// It is in win32_main.c. We might need to forward decl or just return an Action?
// Better: Return "Path to Navigate" or handle it if we can call extern.
extern void ChangeDirectory(wchar_t *Path, b32 PushHistory);
extern void LogMessage(const char *Format, ...);

void
RenderHomeView(f32 X, f32 Y, f32 W, f32 H, b32 Clicked)
{
    extern f32 GlobalScale;
    
    // Draw Title
    DrawTextStrUsingHeight(&GlobalRenderer, &Font, X + 20*GlobalScale, Y + 20*GlobalScale, "This PC", (f32[]){1,1,1,1}, 24.0f * GlobalScale);
    
    // Grid Layout
    f32 Padding = 24.0f * GlobalScale;
    f32 CurrentX = X + Padding;
    f32 CurrentY = Y + 70.0f * GlobalScale;
    f32 CardW = 300.0f * GlobalScale;
    f32 CardH = 120.0f * GlobalScale;
    
    // Iterate Drives
    DWORD Drives = GetLogicalDrives();
    for(char Letter='A'; Letter<='Z'; Letter++)
    {
        if(Drives & 1)
        {
            wchar_t DriveRoot[] = { (wchar_t)Letter, L':', L'\\', 0 };
            UINT Type = GetDriveTypeW(DriveRoot);
            
            if(Type != DRIVE_NO_ROOT_DIR)
            {
                wchar_t VolName[MAX_PATH];
                GetVolumeInformationW(DriveRoot, VolName, MAX_PATH, 0, 0, 0, 0, 0);
                
                ULARGE_INTEGER FreeBytes, TotalBytes, TotalFree;
                b32 HasSpace = GetDiskFreeSpaceExW(DriveRoot, &FreeBytes, &TotalBytes, &TotalFree);
                
                if(CurrentX + CardW > X + W - Padding) {
                    CurrentX = X + Padding;
                    CurrentY += CardH + Padding;
                }
                
                b32 IsHover = (GlobalMouseX >= CurrentX && GlobalMouseX <= CurrentX + CardW &&
                               GlobalMouseY >= CurrentY && GlobalMouseY <= CurrentY + CardH);
                
                // --- Glassmorphism Effect ---
                f32 BgColor[] = {0.2f, 0.2f, 0.22f, 0.4f}; // Semi-transparent
                f32 BorderColor[] = {1, 1, 1, 0.1f};
                if(IsHover && Clicked) {
                    ChangeDirectory(DriveRoot, true);
                }
                
                extern ID3D11ShaderResourceView *GlobalDriveTexture;
                extern f32 GlobalHomeAnimT;
                
                // Animation logic
                f32 BaseScale = GlobalHomeAnimT;
                f32 Scale = BaseScale;
                if(IsHover) Scale *= 1.05f; // Slight pop on hover
                
                f32 AnimW = CardW * Scale;
                f32 AnimH = CardH * Scale;
                f32 AnimX = CurrentX + (CardW - AnimW) * 0.5f;
                f32 AnimY = CurrentY + (CardH - AnimH) * 0.5f;

                // Card Shadow & Base
                f32 Alpha = GlobalHomeAnimT;
                f32 ShadowCol[] = {0,0,0,0.2f * Alpha};
                f32 FinalBg[] = {BgColor[0], BgColor[1], BgColor[2], BgColor[3] * Alpha};
                
                RenderRoundedQuad(&GlobalRenderer, AnimX + 2, AnimY + 2, AnimW, AnimH, 12*GlobalScale, ShadowCol);
                RenderRoundedQuad(&GlobalRenderer, AnimX, AnimY, AnimW, AnimH, 12*GlobalScale, FinalBg);
                
                // Drive Picture (Procedural 8-bit)
                f32 IconX = AnimX + 24*GlobalScale * Scale;
                f32 IconY = AnimY + 16*GlobalScale * Scale;
                
                Render8BitIcon((Type == DRIVE_REMOVABLE || Type == DRIVE_CDROM) ? ICON_DRIVE_EXTERNAL : ICON_DRIVE_INTERNAL, 
                               IconX, IconY, 4.0f * GlobalScale * Scale);
                
                f32 IconS = 16.0f * 4.0f * GlobalScale * Scale; // 4.0 is my 8-bit scale factor
                
                // Label
                char LabelStr[256];
                if(VolName[0]) sprintf_s(LabelStr, 256, "%S", VolName);
                else sprintf_s(LabelStr, 256, (Type == DRIVE_FIXED) ? "Local Disk" : "External Drive");
                
                DrawTextStrUsingHeight(&GlobalRenderer, &Font, IconX + IconS + 12*GlobalScale, IconY + 4*GlobalScale, LabelStr, (f32[]){0.95f, 0.95f, 1, 1}, 18.0f * GlobalScale);
                
                char LetterStr[16];
                sprintf_s(LetterStr, 16, "(%c:)", Letter);
                DrawTextStr(&GlobalRenderer, &Font, IconX + IconS + 12*GlobalScale, IconY + 24*GlobalScale, LetterStr, (f32[]){0.6f, 0.6f, 0.6f, 1.0f});
                
                // Usage Section
                if(HasSpace)
                {
                    f32 Pct = 1.0f - ((f32)TotalFree.QuadPart / (f32)TotalBytes.QuadPart);
                    f32 BarX = CurrentX + 16*GlobalScale;
                    f32 BarY = CurrentY + CardH - 35*GlobalScale;
                    f32 BarW = CardW - 32*GlobalScale;
                    f32 BarH = 8*GlobalScale;
                    
                    // Track
                    RenderRoundedQuad(&GlobalRenderer, BarX, BarY, BarW, BarH, 4*GlobalScale, (f32[]){0.1f, 0.1f, 0.1f, 1.0f});
                    
                    // Monochromatic Silver Fill
                    f32 FillCol[] = {0.75f, 0.75f, 0.75f, 1.0f};
                    if(Pct > 0.85f) { FillCol[0]=0.85f; FillCol[1]=0.85f; FillCol[2]=0.85f; } // Brighter on warning?
                    if(Pct > 0.95f) { FillCol[0]=0.95f; FillCol[1]=0.95f; FillCol[2]=0.95f; } // Pure white if critical
                    
                    RenderRoundedQuad(&GlobalRenderer, BarX, BarY, BarW * Pct, BarH, 4*GlobalScale, FillCol);
                    
                    char StatStr[128];
                    double FreeGB = (double)TotalFree.QuadPart / (1024.0*1024.0*1024.0);
                    double TotalGB = (double)TotalBytes.QuadPart / (1024.0*1024.0*1024.0);
                    sprintf_s(StatStr, 128, "%.1f GB free / %.0f GB", FreeGB, TotalGB);
                    DrawTextStr(&GlobalRenderer, &Font, BarX, BarY + 12*GlobalScale, StatStr, (f32[]){0.5f, 0.5f, 0.5f, 1.0f});
                }
                
                CurrentX += CardW + Padding;
            }
        }
        Drives >>= 1;
    }
}
