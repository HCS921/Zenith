
// --- UI Component Functions ---

void
RenderSidebar(renderer_state *Renderer, font_atlas *Font, f32 X, f32 Y, f32 W, f32 H, 
              int MouseX, int MouseY, f32 *SidebarHoverColor, f32 *TextColor, f32 *MutedColor, f32 *HoverT)
{
    extern f32 GlobalScale;
    f32 SidePadding = 16.0f * GlobalScale;
    f32 CurrentY = Y + 20.0f * GlobalScale;
    
    DrawTextStr(Renderer, Font, X + SidePadding, CurrentY, "PINNED", MutedColor); 
    CurrentY += 32.0f * GlobalScale;
    
    char *Pinned[] = {"Desktop", "Documents", "Downloads", "Music", "Pictures", "Videos"};
    f32 ItemH = 30.0f * GlobalScale;
    
    for(int i=0; i<6; ++i)
    {
        f32 T = HoverT[i];
        f32 Col[4] = {SidebarHoverColor[0], SidebarHoverColor[1], SidebarHoverColor[2], 0.15f * T};
        
        // Animated selection pill
        f32 PillIndent = 8.0f * GlobalScale;
        f32 AnimIndent = (4.0f * T) * GlobalScale;
        
        if(T > 0.01f)
            RenderRoundedQuad(Renderer, X + PillIndent, CurrentY, W - (PillIndent * 2), ItemH, 6.0f*GlobalScale, Col);
        
        DrawTextStr(Renderer, Font, X + SidePadding + AnimIndent, CurrentY + 6*GlobalScale, Pinned[i], TextColor);
        CurrentY += ItemH;
    }
    
    CurrentY += 24.0f * GlobalScale;
    DrawTextStr(Renderer, Font, X + SidePadding, CurrentY, "DRIVES", MutedColor); 
    CurrentY += 32.0f * GlobalScale;
    
    int DriveIndex = 6;
    DWORD Drives = GetLogicalDrives();
    for(char Letter='A'; Letter<='Z'; Letter++)
    {
        if(Drives & 1)
        {
            f32 T = HoverT[DriveIndex++];
            f32 Col[4] = {SidebarHoverColor[0], SidebarHoverColor[1], SidebarHoverColor[2], 0.15f * T};
            f32 AnimIndent = (4.0f * T) * GlobalScale;

            if(T > 0.01f)
                RenderRoundedQuad(Renderer, X + 8*GlobalScale, CurrentY, W - 16*GlobalScale, ItemH, 6.0f*GlobalScale, Col);
            
            wchar_t RootPath[4] = { (wchar_t)Letter, L':', L'\\', 0 };
            UINT DriveType = GetDriveTypeW(RootPath);
            icon_type IType = (DriveType == DRIVE_REMOVABLE || DriveType == DRIVE_CDROM) ? ICON_DRIVE_EXTERNAL : ICON_DRIVE_INTERNAL;
            Render8BitIcon(IType, X + SidePadding + AnimIndent, CurrentY + 8*GlobalScale, GlobalScale);
            
            char DriveName[16];
            sprintf_s(DriveName, sizeof(DriveName), "%c: Drive", Letter);
            DrawTextStr(Renderer, Font, X + SidePadding + AnimIndent + 24*GlobalScale, CurrentY + 6*GlobalScale, DriveName, TextColor);
            
            // Usage Bar
            f32 BarW = W - (SidePadding * 2 + 16*GlobalScale);
            f32 BarY = CurrentY + ItemH - 4*GlobalScale;
            
            // Calculate Usage
            ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
            f32 UsagePct = 0.0f;
            if(GetDiskFreeSpaceExW(RootPath, &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes))
            {
                if(TotalNumberOfBytes.QuadPart > 0)
                {
                    u64 Used = TotalNumberOfBytes.QuadPart - TotalNumberOfFreeBytes.QuadPart;
                    UsagePct = (f32)((double)Used / (double)TotalNumberOfBytes.QuadPart);
                }
            }
            
            f32 BarBg[] = {0.18f, 0.18f, 0.18f, 1.0f};
            f32 BarFg[] = {0.8f, 0.8f, 0.8f, 0.6f + (0.4f * T)}; 
            
            // Color Shift based on fullness? (Optional)
            if(UsagePct > 0.9f) { BarFg[0]=0.9f; BarFg[1]=0.2f; BarFg[2]=0.2f; } // Red if full
            
            RenderQuad(Renderer, X + SidePadding + AnimIndent + 8*GlobalScale, BarY, BarW, 2*GlobalScale, BarBg);
            RenderQuad(Renderer, X + SidePadding + AnimIndent + 8*GlobalScale, BarY, BarW * UsagePct, 2*GlobalScale, BarFg);
            
            CurrentY += ItemH;
        }
        Drives >>= 1;
    }
}

// RenderPreviewPane removed (Replaced by Inspector_Draw)
