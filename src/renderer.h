#ifndef RENDERER_H
#define RENDERER_H

#include "defs.h"
#include <d3d11.h>
#include <d3dcompiler.h>

typedef struct renderer_state
{
    ID3D11Device *Device;
    ID3D11DeviceContext *Context;
    IDXGISwapChain *SwapChain;
    ID3D11RenderTargetView *BackBufferView;
    ID3D11DepthStencilState *DepthState;
    
    // Shader Resources
    ID3D11VertexShader *VertexShader;
    ID3D11PixelShader *PixelShader;
    ID3D11InputLayout *InputLayout;
    
    // Batching State
    ID3D11Buffer *VertexBuffer;
    ID3D11Buffer *ConstantBuffer;
    struct vertex *MappedVertices;
    u32 VertexCount;
    ID3D11ShaderResourceView *CurrentTexture;
    
    // Font / Texture
    ID3D11ShaderResourceView *FontTextureView;
    ID3D11ShaderResourceView *WhiteTextureView;
    ID3D11SamplerState *Sampler;
    ID3D11BlendState *BlendState;
    
    // Transform / Alpha Stack (Simple 1-level for now)
    f32 GlobalOffsetX;
    f32 GlobalOffsetY;
    f32 GlobalAlpha;
} renderer_state;

typedef struct vertex
{
    f32 Pos[3];
    f32 UV[2];
    f32 Color[4];
    f32 Params[3]; // W, H, Radius
} vertex;

typedef struct constant_buffer
{
    f32 Projection[4][4];
} constant_buffer;

// Init
internal b32 InitD3D11(HWND Window, renderer_state *State);
internal void ResizeD3D11(renderer_state *State, f32 Width, f32 Height);
// Draw
internal void BeginFrame(renderer_state *State, f32 WindowWidth, f32 WindowHeight);
internal void EndFrame(renderer_state *State);
internal void RenderQuad(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, f32 Color[4]);
internal void RenderRoundedQuad(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, f32 Radius, f32 Color[4]);
internal void RenderQuadUV(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, f32 U0, f32 V0, f32 U1, f32 V1, f32 Color[4]);
internal void Flush(renderer_state *State);
internal void SetTexture(renderer_state *State, ID3D11ShaderResourceView *View);
internal void RenderQuadWithTexture(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, ID3D11ShaderResourceView *Texture);

// Loader
internal ID3D11ShaderResourceView *LoadTextureFromWIC(renderer_state *State, wchar_t *Filename);


#endif // RENDERER_H
