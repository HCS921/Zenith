#ifndef INSPECTOR_H
#define INSPECTOR_H

#include "defs.h"
#include "renderer.h"
#include "filesystem.h"
#include "font.h"

// Constants
#define UI_GUTTER 12.0f
#define ICON_RADIUS 4.0f

void Inspector_Draw(renderer_state *Renderer, font_atlas *Font, f32 X, f32 Y, f32 W, f32 H,
                    int MouseX, int MouseY, f32 *TextColor, f32 *MutedColor, f32 *SepColor, 
                    directory_list *PreviewList);

#endif
