#ifndef FONT_C
#define FONT_C

#include "font.h"
#include <stdio.h> // for sprintf

internal b32
InitFontAtlas(renderer_state *Renderer, font_atlas *Atlas, char *FontName, int FontSize)
{
    // Standard Raster Atlas
    int TexWidth = 512;
    int TexHeight = 512;
    
    HDC DC = CreateCompatibleDC(0);
    BITMAPINFO Info = {0};
    Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
    Info.bmiHeader.biWidth = TexWidth;
    Info.bmiHeader.biHeight = -TexHeight;
    Info.bmiHeader.biPlanes = 1;
    Info.bmiHeader.biBitCount = 32;
    Info.bmiHeader.biCompression = BI_RGB;
    
    void *Bits = 0;
    HBITMAP Bitmap = CreateDIBSection(DC, &Info, DIB_RGB_COLORS, &Bits, 0, 0);
    SelectObject(DC, Bitmap);
    
    memset(Bits, 0, TexWidth * TexHeight * 4);
    
    HFONT Font = CreateFontA(FontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
        ANTIALIASED_QUALITY, DEFAULT_PITCH|FF_DONTCARE, FontName);
    
    SelectObject(DC, Font);
    SetTextColor(DC, 0x00FFFFFF);
    SetBkMode(DC, TRANSPARENT);
    
    TEXTMETRICA TM;
    GetTextMetricsA(DC, &TM);
    Atlas->LineHeight = (f32)TM.tmHeight;
    
    int CurrentX = 0;
    int CurrentY = 0;
    int RowHeight = TM.tmHeight + 2;
    
    for(int CharCode = 32; CharCode < 127; ++CharCode)
    {
        char Str[2] = {(char)CharCode, 0};
        SIZE Size;
        GetTextExtentPoint32A(DC, Str, 1, &Size);
        
        if(CurrentX + Size.cx >= TexWidth)
        {
            CurrentX = 0;
            CurrentY += RowHeight;
        }
        
        TextOutA(DC, CurrentX, CurrentY, Str, 1);
        
        glyph_info *Glyph = &Atlas->Glyphs[CharCode];
        Glyph->U0 = (f32)CurrentX / TexWidth;
        Glyph->V0 = (f32)CurrentY / TexHeight;
        Glyph->U1 = (f32)(CurrentX + Size.cx) / TexWidth;
        Glyph->V1 = (f32)(CurrentY + Size.cy) / TexHeight;
        Glyph->Width = (f32)Size.cx;
        Glyph->Height = (f32)Size.cy;
        Glyph->XAdvance = (f32)Size.cx;
        
        Glyph->XOffset = 0;
        Glyph->YOffset = 0;
        
        CurrentX += Size.cx + 2;
    }

    // Alpha Post-Processing
    u32 *Pixels = (u32 *)Bits;
    for(int i = 0; i < TexWidth * TexHeight; ++i)
    {
        u32 C = Pixels[i];
        u8 R = (C >> 16) & 0xFF;
        if(R > 0) Pixels[i] = (R << 24) | 0x00FFFFFF;
        else Pixels[i] = 0;
    }
    
    D3D11_TEXTURE2D_DESC TexDesc = {0};
    TexDesc.Width = TexWidth;
    TexDesc.Height = TexHeight;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 1;
    TexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    TexDesc.SampleDesc.Count = 1;
    TexDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA Data;
    Data.pSysMem = Bits;
    Data.SysMemPitch = TexWidth * 4;
    Data.SysMemSlicePitch = 0;
    
    Renderer->Device->lpVtbl->CreateTexture2D(Renderer->Device, &TexDesc, &Data, &Atlas->Texture);
    Renderer->Device->lpVtbl->CreateShaderResourceView(Renderer->Device, (ID3D11Resource*)Atlas->Texture, 0, &Atlas->View);
    
    DeleteObject(Bitmap);
    DeleteObject(Font);
    DeleteDC(DC);
    
    D3D11_SAMPLER_DESC SampDesc = {0};
    SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    Renderer->Device->lpVtbl->CreateSamplerState(Renderer->Device, &SampDesc, &Renderer->Sampler);
    
    return true;
}

// We need a way to draw Textured Quads.
// We can overload RenderQuad or make a RenderQuadUV.
// For "Quick and Dirty C", I'll just access the renderer internals or add a function to renderer.c
// But since this is font.c, I can't easily modify renderer internal state unless exposed.
// Renderer state has MappedVertices exposed? No it's static in renderer.c.
// I should add RenderQuadUV to renderer.h/c.
// For now, I'll assume it exists and implement I'll add it to renderer.c in a moment.

internal void
DrawTextStr(renderer_state *Renderer, font_atlas *Atlas, f32 X, f32 Y, char *Text, f32 Color[4])
{
    SetTexture(Renderer, Atlas->View);
    f32 CurX = X;
    f32 CurY = Y;
    
    while(*Text)
    {
        int CharCode = (u8)*Text++;
        if(CharCode >= 32 && CharCode < 127)
        {
            glyph_info *Glyph = &Atlas->Glyphs[CharCode];
            RenderQuadUV(Renderer, CurX, CurY, Glyph->Width, Glyph->Height, Glyph->U0, Glyph->V0, Glyph->U1, Glyph->V1, Color);
            CurX += Glyph->XAdvance;
        }
    }
}

internal void
DrawTextStrUsingHeight(renderer_state *Renderer, font_atlas *Atlas, f32 X, f32 Y, char *Text, f32 Color[4], f32 Height)
{
    f32 Scale = Height / Atlas->LineHeight;
    SetTexture(Renderer, Atlas->View);
    f32 CurX = X;
    f32 CurY = Y;
    
    while(*Text)
    {
        int CharCode = (u8)*Text++;
        if(CharCode >= 32 && CharCode < 127)
        {
            glyph_info *Glyph = &Atlas->Glyphs[CharCode];
            RenderQuadUV(Renderer, CurX, CurY, Glyph->Width * Scale, Glyph->Height * Scale, Glyph->U0, Glyph->V0, Glyph->U1, Glyph->V1, Color);
            CurX += Glyph->XAdvance * Scale;
        }
    }
}

#endif // FONT_C
