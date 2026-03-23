#ifndef FONT_H
#define FONT_H

#include "defs.h"
#include "renderer.h"

#define MAX_GLYPHS 128

typedef struct glyph_info
{
    f32 U0, V0, U1, V1;
    f32 Width, Height;
    f32 XOffset, YOffset;
    f32 XAdvance;
} glyph_info;

typedef struct font_atlas
{
    ID3D11Texture2D *Texture;
    ID3D11ShaderResourceView *View;
    glyph_info Glyphs[MAX_GLYPHS];
    f32 LineHeight;
} font_atlas;

// Generates a font atlas using GDI and uploads to GPU
internal b32 InitFontAtlas(renderer_state *Renderer, font_atlas *Atlas, char *FontName, int Size);

// Draws text using the atlas
internal void DrawTextStr(renderer_state *Renderer, font_atlas *Atlas, f32 X, f32 Y, char *Text, f32 Color[4]);
internal void DrawTextStrUsingHeight(renderer_state *Renderer, font_atlas *Atlas, f32 X, f32 Y, char *Text, f32 Color[4], f32 Height);

#endif // FONT_H
