// ----------------------------------------------------------------------------
#include "defs.h"
#include "app_interface.h" 
// (Ensure GlobalRenderer is visible here or declare extern)
extern renderer_state GlobalRenderer; 

// ----------------------------------------------------------------------------
static void 
Render8BitDot(f32 X, f32 Y, f32 Size, f32 *Color)
{
    RenderQuad(&GlobalRenderer, X, Y, Size, Size, Color);
}

void
Render8BitIcon(icon_type Type, f32 X, f32 Y, f32 Scale)
{
    f32 Pixel = 1.0f * Scale;
    
    switch(Type)
    {
        case ICON_FOLDER:
        {
            f32 Col[] = {0.95f, 0.75f, 0.2f, 1.0f}; // Manila Yellow
            f32 ShadowCol[] = {0.6f, 0.45f, 0.1f, 1.0f};
            f32 LightCol[] = {1.0f, 0.9f, 0.4f, 1.0f};
            
            // Tab
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 1*Pixel, 5*Pixel, 3*Pixel, ShadowCol);
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 2*Pixel, 4*Pixel, 2*Pixel, Col);
            // Main body
            RenderQuad(&GlobalRenderer, X + 0*Pixel, Y + 4*Pixel, 16*Pixel, 10*Pixel, ShadowCol);
            RenderQuad(&GlobalRenderer, X + 0*Pixel, Y + 4*Pixel, 15*Pixel, 9*Pixel, Col);
            // Detail (Highlight on top edge)
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 4*Pixel, 14*Pixel, 1*Pixel, LightCol);
        } break;
        
        case ICON_DRIVE_INTERNAL:
        {
            f32 Silver[] = {0.8f, 0.8f, 0.8f, 1.0f};
            f32 DarkSilver[] = {0.35f, 0.35f, 0.35f, 1.0f};
            f32 LED[] = {0.0f, 1.0f, 0.0f, 1.0f}; // Green LED
            
            // Chassis
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 2*Pixel, 14*Pixel, 12*Pixel, DarkSilver);
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 3*Pixel, 12*Pixel, 10*Pixel, Silver);
            // Top Cover line
            RenderQuad(&GlobalRenderer, X + 3*Pixel, Y + 4*Pixel, 10*Pixel, 1*Pixel, (f32[]){0.2f, 0.6f, 1.0f, 1}); // Blue Stripe
            // LED
            RenderQuad(&GlobalRenderer, X + 12*Pixel, Y + 11*Pixel, 2*Pixel, 1*Pixel, LED);
        } break;

        case ICON_DRIVE_EXTERNAL:
        {
             f32 Shell[] = {0.12f, 0.12f, 0.12f, 1.0f}; // Obsidian Shell
             f32 Accent[] = {0.2f, 0.6f, 0.9f, 1.0f};   // Cyan/Blue Accent
             f32 LED[] = {0.2f, 0.8f, 1.0f, 1.0f};     // Cyan LED
             
             RenderQuad(&GlobalRenderer, X + 3*Pixel, Y + 1*Pixel, 10*Pixel, 14*Pixel, Shell);
             RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 2*Pixel, 8*Pixel, 1*Pixel, Accent); // Top highlight
             RenderQuad(&GlobalRenderer, X + 7*Pixel, Y + 12*Pixel, 2*Pixel, 1*Pixel, LED); // LED
        } break;
        
        case ICON_FILE_TEXT:
        {
            f32 White[] = {0.95f, 0.95f, 0.95f, 1.0f};
            f32 Lines[] = {0.6f, 0.6f, 0.6f, 0.8f}; // Soft mono lines
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 1*Pixel, 12*Pixel, 14*Pixel, White);
            // Folded corner
            RenderQuad(&GlobalRenderer, X + 11*Pixel, Y + 1*Pixel, 3*Pixel, 3*Pixel, (f32[]){0.9f, 0.9f, 0.8f, 1}); // Slight cream
            // Text Lines
            for(int i=0; i<4; ++i)
                RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 6*Pixel + (i*2)*Pixel, (i == 3 ? 5 : 8)*Pixel, 1*Pixel, Lines);
        } break;
        
        case ICON_FILE_EXE:
        {
            f32 Frame[] = {0.2f, 0.2f, 0.25f, 1.0f}; // Blue-ish dark frame
            f32 Window[] = {0.9f, 0.9f, 0.9f, 1.0f};    // White window
            f32 Play[] = {0.2f, 0.8f, 0.2f, 1.0f};      // Green Play button
            
            // The Window Frame
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 1*Pixel, 14*Pixel, 14*Pixel, Frame);
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 4*Pixel, 12*Pixel, 10*Pixel, Window);
            // Header buttons
            RenderQuad(&GlobalRenderer, X + 3*Pixel, Y + 2*Pixel, 1*Pixel, 1*Pixel, (f32[]){0.8f, 0.3f, 0.3f, 1}); // Red dot
            RenderQuad(&GlobalRenderer, X + 5*Pixel, Y + 2*Pixel, 3*Pixel, 1*Pixel, (f32[]){1.0f, 1.0f, 1.0f, 0.5f}); 
            
            // Play Triangle
            for(int i=0; i<4; ++i)
                RenderQuad(&GlobalRenderer, X + 6*Pixel + i*Pixel, Y + 6*Pixel + i*Pixel, 1*Pixel, (7 - i*2)*Pixel, Play);
        } break;
        
        case ICON_FILE_DLL:
        {
            f32 Bg[] = {0.3f, 0.3f, 0.5f, 1.0f}; // Purple/Blue
            f32 Gear[] = {0.9f, 0.8f, 0.4f, 1.0f}; // Gold Gear
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 2*Pixel, 12*Pixel, 12*Pixel, Bg);
            // More detailed gear
            RenderQuad(&GlobalRenderer, X + 7*Pixel, Y + 1*Pixel, 2*Pixel, 14*Pixel, Gear); // Vertical
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 7*Pixel, 14*Pixel, 2*Pixel, Gear); // Horizontal
            RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 4*Pixel, 8*Pixel, 8*Pixel, Gear);   // Ring
            RenderQuad(&GlobalRenderer, X + 6*Pixel, Y + 6*Pixel, 4*Pixel, 4*Pixel, Bg);     // Hub
        } break;
        
        case ICON_FILE_CODE:
        {
             f32 Bg[] = {0.1f, 0.1f, 0.12f, 1.0f};
             f32 Col[] = {0.3f, 0.8f, 0.3f, 1.0f}; // Matrix Green Syntax
             RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 1*Pixel, 12*Pixel, 14*Pixel, Bg);
             // < >
             RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 6*Pixel, 1*Pixel, 4*Pixel, Col); // Left
             RenderQuad(&GlobalRenderer, X + 5*Pixel, Y + 5*Pixel, 1*Pixel, 1*Pixel, Col);
             RenderQuad(&GlobalRenderer, X + 5*Pixel, Y + 10*Pixel, 1*Pixel, 1*Pixel, Col);
             
             RenderQuad(&GlobalRenderer, X + 11*Pixel, Y + 6*Pixel, 1*Pixel, 4*Pixel, Col); // Right
             RenderQuad(&GlobalRenderer, X + 10*Pixel, Y + 5*Pixel, 1*Pixel, 1*Pixel, Col);
             RenderQuad(&GlobalRenderer, X + 10*Pixel, Y + 10*Pixel, 1*Pixel, 1*Pixel, Col);
             
             RenderQuad(&GlobalRenderer, X + 8*Pixel, Y + 4*Pixel, 1*Pixel, 8*Pixel, (f32[]){1.0f, 1.0f, 1.0f, 1}); // / Slash
        } break;
        
        case ICON_FILE_IMAGE:
        {
             f32 Sky[] = {0.4f, 0.7f, 1.0f, 1.0f}; // Blue Sky
             f32 Sun[] = {1.0f, 0.9f, 0.2f, 1.0f}; // Yellow Sun
             f32 PeakCol[] = {1.0f, 1.0f, 1.0f, 1.0f};
             f32 Land[] = {0.2f, 0.8f, 0.3f, 1.0f}; // Green Land
             f32 MountCol[] = {0.5f, 0.4f, 0.3f, 1.0f}; // Brown Mount
             
             RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 2*Pixel, 14*Pixel, 11*Pixel, Sky);
             RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 11*Pixel, 14*Pixel, 3*Pixel, Land);
             RenderQuad(&GlobalRenderer, X + 11*Pixel, Y + 4*Pixel, 2*Pixel, 2*Pixel, Sun);
             // Mountains
             RenderQuad(&GlobalRenderer, X + 3*Pixel, Y + 8*Pixel, 4*Pixel, 5*Pixel, MountCol);
             RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 7*Pixel, 2*Pixel, 1*Pixel, PeakCol);
             RenderQuad(&GlobalRenderer, X + 8*Pixel, Y + 6*Pixel, 5*Pixel, 7*Pixel, (f32[]){0.4f, 0.3f, 0.2f, 1});
             RenderQuad(&GlobalRenderer, X + 10*Pixel, Y + 5*Pixel, 1*Pixel, 1*Pixel, PeakCol);
        } break;
        
        case ICON_FILE_PYTHON:
        {
            f32 DarkGray[] = {0.3f, 0.3f, 0.3f, 1.0f};
            f32 LightGray[] = {0.7f, 0.7f, 0.7f, 1.0f};
            f32 WhiteCol[] = {1.0f, 1.0f, 1.0f, 1.0f};
            
            // Top Snake (Dark)
            RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 2*Pixel, 8*Pixel, 6*Pixel, DarkGray);
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 4*Pixel, 2*Pixel, 4*Pixel, DarkGray);
            // Bottom Snake (Light)
            RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 8*Pixel, 8*Pixel, 6*Pixel, LightGray);
            RenderQuad(&GlobalRenderer, X + 12*Pixel, Y + 8*Pixel, 2*Pixel, 4*Pixel, LightGray);
            
            RenderQuad(&GlobalRenderer, X + 6*Pixel, Y + 4*Pixel, 1*Pixel, 1*Pixel, WhiteCol); // Eye
            RenderQuad(&GlobalRenderer, X + 9*Pixel, Y + 11*Pixel, 1*Pixel, 1*Pixel, WhiteCol); // Eye 2
        } break;
        
        default: // Generic File
        {
            f32 Col[] = {0.9f, 0.9f, 0.9f, 1.0f};
            f32 DarkGray[] = {0.7f, 0.7f, 0.7f, 1.0f};
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 1*Pixel, 12*Pixel, 14*Pixel, Col);
            RenderQuad(&GlobalRenderer, X + 11*Pixel, Y + 1*Pixel, 3*Pixel, 3*Pixel, DarkGray);
        } break;

        case ICON_APP_FACE:
        {
            f32 MonitorCol[] = {0.7f, 0.7f, 0.7f, 1.0f}; // Chalk Gray
            f32 ScreenCol[] = {0.15f, 0.15f, 0.15f, 1.0f}; // Charcoal
            f32 EyeCol[] = {1.0f, 1.0f, 1.0f, 1.0f};      // Pure White
            f32 RimCol[] = {1.0f, 1.0f, 1.0f, 0.4f};     
            
            // Chassis & Highlights
            RenderQuad(&GlobalRenderer, X + 1*Pixel, Y + 1*Pixel, 14*Pixel, 12*Pixel, MonitorCol);
            RenderQuad(&GlobalRenderer, X + 2*Pixel, Y + 2*Pixel, 12*Pixel, 10*Pixel, ScreenCol);
            
            // Glowing Eyes (Large & prominent)
            RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 5*Pixel, 2*Pixel, 2*Pixel, EyeCol);
            RenderQuad(&GlobalRenderer, X + 10*Pixel, Y + 5*Pixel, 2*Pixel, 2*Pixel, EyeCol);
            
            // Base & Neck
            RenderQuad(&GlobalRenderer, X + 6*Pixel, Y + 13*Pixel, 4*Pixel, 1*Pixel, MonitorCol);
            RenderQuad(&GlobalRenderer, X + 4*Pixel, Y + 14*Pixel, 8*Pixel, 1*Pixel, MonitorCol);
        } break;

        case ICON_SEARCH: {
        f32 Col[] = {0.8f, 0.8f, 0.8f, 1.0f}; // Silver Search Icon
        f32 Handle[] = {0.55f, 0.55f, 0.55f, 1.0f}; 
        
        // Ring
        Render8BitDot(X+4*Pixel, Y+2*Pixel, Pixel, Col); Render8BitDot(X+5*Pixel, Y+2*Pixel, Pixel, Col); Render8BitDot(X+6*Pixel, Y+2*Pixel, Pixel, Col);
        Render8BitDot(X+3*Pixel, Y+3*Pixel, Pixel, Col); Render8BitDot(X+7*Pixel, Y+3*Pixel, Pixel, Col);
        Render8BitDot(X+2*Pixel, Y+4*Pixel, Pixel, Col); Render8BitDot(X+8*Pixel, Y+4*Pixel, Pixel, Col);
        Render8BitDot(X+2*Pixel, Y+5*Pixel, Pixel, Col); Render8BitDot(X+8*Pixel, Y+5*Pixel, Pixel, Col);
        Render8BitDot(X+2*Pixel, Y+6*Pixel, Pixel, Col); Render8BitDot(X+8*Pixel, Y+6*Pixel, Pixel, Col);
        Render8BitDot(X+3*Pixel, Y+7*Pixel, Pixel, Col); Render8BitDot(X+7*Pixel, Y+7*Pixel, Pixel, Col);
        Render8BitDot(X+4*Pixel, Y+8*Pixel, Pixel, Col); Render8BitDot(X+5*Pixel, Y+8*Pixel, Pixel, Col); Render8BitDot(X+6*Pixel, Y+8*Pixel, Pixel, Col);
        
        // Handle
        Render8BitDot(X+8*Pixel, Y+8*Pixel, Pixel, Handle); Render8BitDot(X+9*Pixel, Y+9*Pixel, Pixel, Handle); Render8BitDot(X+10*Pixel, Y+10*Pixel, Pixel, Handle);
        Render8BitDot(X+11*Pixel, Y+11*Pixel, Pixel, Handle);
    } break;
    }
}

// ----------------------------------------------------------------------------
// DetermineIconType: Central helper to map file extensions to icon types.
// Used across list views (Storage, Home, File List) for consistency.
// ----------------------------------------------------------------------------
icon_type
DetermineIconType(wchar_t *Name, b32 IsDirectory)
{
    if(IsDirectory) return ICON_FOLDER;
    
    wchar_t *Ext = wcsrchr(Name, L'.');
    if(Ext) {
        if(lstrcmpiW(Ext, L".py") == 0) return ICON_FILE_PYTHON;
        else if(lstrcmpiW(Ext, L".txt") == 0) return ICON_FILE_TEXT;
        else if(lstrcmpiW(Ext, L".exe") == 0) return ICON_FILE_EXE;
        else if(lstrcmpiW(Ext, L".dll") == 0) return ICON_FILE_DLL;
        else if(lstrcmpiW(Ext, L".c") == 0 || lstrcmpiW(Ext, L".h") == 0 || lstrcmpiW(Ext, L".cpp") == 0 || lstrcmpiW(Ext, L".cs") == 0) return ICON_FILE_CODE;
        else if(lstrcmpiW(Ext, L".png") == 0 || lstrcmpiW(Ext, L".jpg") == 0 || lstrcmpiW(Ext, L".jpeg") == 0 || lstrcmpiW(Ext, L".bmp") == 0) return ICON_FILE_IMAGE;
    }
    
    return ICON_FILE_GENERIC;
}

