#ifndef RENDERER_C
#define RENDERER_C

#include "renderer.h"
#include <wincodec.h>

// Simple Shader Source
// Shader Source with SDF Logic
const char *ShaderSource = 
"cbuffer constants : register(b0) { float4x4 Projection; };\n"
"Texture2D myTex : register(t0);\n"
"SamplerState mySampler : register(s0);\n"
"struct vs_input { float3 Pos : POSITION; float2 UV : TEXCOORD; float4 Color : COLOR; float3 Params : PARAMS; };\n"
"struct ps_input { float4 Pos : SV_POSITION; float2 UV : TEXCOORD; float4 Color : COLOR; float3 Params : PARAMS; float2 LocalPos : LOCALPOS; };\n"
"ps_input vs_main(vs_input input) {\n"
"    ps_input output;\n"
"    output.Pos = mul(float4(input.Pos, 1.0), Projection);\n"
"    output.UV = input.UV;\n"
"    output.Color = input.Color;\n"
"    output.Params = input.Params;\n"
"    // Calculate LocalPos assuming UV 0..1 maps to 0..Params.xy\n"
"    output.LocalPos = input.UV * input.Params.xy;\n"
"    return output;\n"
"}\n"
"float rounded_box(float2 p, float2 b, float r) {\n"
"    return length(max(abs(p)-b+r, 0.0)) - r;\n"
"}\n"
"float4 ps_main(ps_input input) : SV_TARGET {\n"
"    float4 tex = myTex.Sample(mySampler, input.UV);\n"
"    float alpha = 1.0;\n"
"    if (input.Params.z > 0.0) {\n"
"        float2 size = input.Params.xy;\n"
"        float radius = input.Params.z;\n"
"        float2 p = input.LocalPos - (size * 0.5);\n"
"        float dist = rounded_box(p, (size * 0.5), radius);\n"
"        alpha = 1.0 - smoothstep(-0.5, 0.5, dist);\n"
"    }\n"
"    return input.Color * tex * float4(1,1,1,alpha);\n"
"}\n";


internal b32
InitD3D11(HWND Window, renderer_state *State)
{
    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {0};
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2; // Double buffering
    SwapChainDesc.OutputWindow = Window;
    SwapChainDesc.Windowed = true;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    SwapChainDesc.Flags = 0;

    UINT Flags = 0;
#ifdef _DEBUG
    Flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT HR = D3D11CreateDeviceAndSwapChain(
        0, D3D_DRIVER_TYPE_HARDWARE, 0, Flags, FeatureLevels, ArrayCount(FeatureLevels),
        D3D11_SDK_VERSION, &SwapChainDesc, &State->SwapChain, &State->Device, 0, &State->Context);

    if(FAILED(HR))
    {
#ifdef _DEBUG
        HR = D3D11CreateDeviceAndSwapChain(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, FeatureLevels, ArrayCount(FeatureLevels),
            D3D11_SDK_VERSION, &SwapChainDesc, &State->SwapChain, &State->Device, 0, &State->Context);
        if(FAILED(HR)) return false;
#else
        return false;
#endif
    }

    // Render Target
    ID3D11Texture2D *BackBuffer;
    State->SwapChain->lpVtbl->GetBuffer(State->SwapChain, 0, &IID_ID3D11Texture2D, (void**)&BackBuffer);
    State->Device->lpVtbl->CreateRenderTargetView(State->Device, (ID3D11Resource*)BackBuffer, 0, &State->BackBufferView);
    BackBuffer->lpVtbl->Release(BackBuffer);

    // Initial Viewport (Can be reset later)
    RECT Rect;
    GetClientRect(Window, &Rect);
    D3D11_VIEWPORT Viewport = {0};
    Viewport.Width = (f32)(Rect.right - Rect.left);
    Viewport.Height = (f32)(Rect.bottom - Rect.top);
    Viewport.MaxDepth = 1.0f;
    State->Context->lpVtbl->RSSetViewports(State->Context, 1, &Viewport);

    // Shaders
    ID3DBlob *VSBlob = 0;
    ID3DBlob *PSBlob = 0;
    ID3DBlob *ErrorBlob = 0;

    HR = D3DCompile(ShaderSource, lstrlenA(ShaderSource), 0, 0, 0, "vs_main", "vs_5_0", 0, 0, &VSBlob, &ErrorBlob);
    if(FAILED(HR)) 
    {
        if(ErrorBlob) OutputDebugStringA((char*)ErrorBlob->lpVtbl->GetBufferPointer(ErrorBlob));
        return false;
    }
    
    HR = D3DCompile(ShaderSource, lstrlenA(ShaderSource), 0, 0, 0, "ps_main", "ps_5_0", 0, 0, &PSBlob, &ErrorBlob);
    if(FAILED(HR))
    {
       if(ErrorBlob) OutputDebugStringA((char*)ErrorBlob->lpVtbl->GetBufferPointer(ErrorBlob));
       return false;
    }

    State->Device->lpVtbl->CreateVertexShader(State->Device, VSBlob->lpVtbl->GetBufferPointer(VSBlob), VSBlob->lpVtbl->GetBufferSize(VSBlob), 0, &State->VertexShader);
    State->Device->lpVtbl->CreatePixelShader(State->Device, PSBlob->lpVtbl->GetBufferPointer(PSBlob), PSBlob->lpVtbl->GetBufferSize(PSBlob), 0, &State->PixelShader);

    // Input Layout
    D3D11_INPUT_ELEMENT_DESC LayoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "PARAMS",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    State->Device->lpVtbl->CreateInputLayout(State->Device, LayoutDesc, ArrayCount(LayoutDesc), VSBlob->lpVtbl->GetBufferPointer(VSBlob), VSBlob->lpVtbl->GetBufferSize(VSBlob), &State->InputLayout);

    VSBlob->lpVtbl->Release(VSBlob);
    PSBlob->lpVtbl->Release(PSBlob);

    // Buffers
    // Vertex Buffer (Dynamic)
    D3D11_BUFFER_DESC VBDesc = {0};
    VBDesc.ByteWidth = sizeof(vertex) * 1024 * 6; // ~1024 quads batch
    VBDesc.Usage = D3D11_USAGE_DYNAMIC;
    VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    State->Device->lpVtbl->CreateBuffer(State->Device, &VBDesc, 0, &State->VertexBuffer);

    // Constant Buffer
    D3D11_BUFFER_DESC CBDesc = {0};
    CBDesc.ByteWidth = sizeof(constant_buffer); // Must be 16 byte aligned
    CBDesc.Usage = D3D11_USAGE_DYNAMIC;
    CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    State->Device->lpVtbl->CreateBuffer(State->Device, &CBDesc, 0, &State->ConstantBuffer);

    // Blend State
    D3D11_BLEND_DESC BlendDesc = {0};
    BlendDesc.RenderTarget[0].BlendEnable = true;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    State->Device->lpVtbl->CreateBlendState(State->Device, &BlendDesc, &State->BlendState);

    // White Texture
    u32 WhitePixels = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC WhiteDesc = {0};
    WhiteDesc.Width = 1;
    WhiteDesc.Height = 1;
    WhiteDesc.MipLevels = 1;
    WhiteDesc.ArraySize = 1;
    WhiteDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    WhiteDesc.SampleDesc.Count = 1;
    WhiteDesc.Usage = D3D11_USAGE_IMMUTABLE;
    WhiteDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA WhiteData = {&WhitePixels, 4, 0};
    ID3D11Texture2D *WhiteTex;
    State->Device->lpVtbl->CreateTexture2D(State->Device, &WhiteDesc, &WhiteData, &WhiteTex);
    State->Device->lpVtbl->CreateShaderResourceView(State->Device, (ID3D11Resource*)WhiteTex, 0, &State->WhiteTextureView);
    State->FontTextureView = State->WhiteTextureView; // Default
    State->CurrentTexture = 0;
    WhiteTex->lpVtbl->Release(WhiteTex);

    return true;
}




// Batch State Constants
static u32 MaxVertexCount = 1024 * 6;

internal void
Flush(renderer_state *State)
{
    if(State->VertexCount > 0)
    {
        State->Context->lpVtbl->Unmap(State->Context, (ID3D11Resource*)State->VertexBuffer, 0);
        
        State->Context->lpVtbl->PSSetShaderResources(State->Context, 0, 1, &State->CurrentTexture);
        State->Context->lpVtbl->Draw(State->Context, State->VertexCount, 0);
        
        State->VertexCount = 0;
        D3D11_MAPPED_SUBRESOURCE MappedVB;
        State->Context->lpVtbl->Map(State->Context, (ID3D11Resource*)State->VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVB);
        State->MappedVertices = (vertex*)MappedVB.pData;
    }
}

internal void
SetTexture(renderer_state *State, ID3D11ShaderResourceView *View)
{
    if(State->CurrentTexture != View)
    {
        Flush(State);
        State->CurrentTexture = View;
    }
}

internal void
BeginFrame(renderer_state *State, f32 WindowWidth, f32 WindowHeight)
{
    if(WindowWidth <= 0) WindowWidth = 1;
    if(WindowHeight <= 0) WindowHeight = 1;
    
    // Reset Global Transform
    State->GlobalOffsetX = 0;
    State->GlobalOffsetY = 0;
    State->GlobalAlpha = 1.0f;

    f32 ClearColor[] = { 1.0f, 0.0f, 0.0f, 1.0f }; // DEBUG RED
    State->Context->lpVtbl->ClearRenderTargetView(State->Context, State->BackBufferView, ClearColor);

    // Update Viewport to match current window size
    D3D11_VIEWPORT Viewport = {0};
    Viewport.Width = WindowWidth;
    Viewport.Height = WindowHeight;
    Viewport.MaxDepth = 1.0f;
    State->Context->lpVtbl->RSSetViewports(State->Context, 1, &Viewport);

    // Setup Pipeline
    State->Context->lpVtbl->IASetInputLayout(State->Context, State->InputLayout);
    State->Context->lpVtbl->IASetPrimitiveTopology(State->Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    UINT Stride = sizeof(vertex);
    UINT Offset = 0;
    State->Context->lpVtbl->IASetVertexBuffers(State->Context, 0, 1, &State->VertexBuffer, &Stride, &Offset);
    
    State->Context->lpVtbl->VSSetShader(State->Context, State->VertexShader, 0, 0);
    State->Context->lpVtbl->PSSetShader(State->Context, State->PixelShader, 0, 0);
    
    State->Context->lpVtbl->OMSetBlendState(State->Context, State->BlendState, 0, 0xFFFFFFFF);
    State->Context->lpVtbl->OMSetRenderTargets(State->Context, 1, &State->BackBufferView, 0);
    State->Context->lpVtbl->PSSetSamplers(State->Context, 0, 1, &State->Sampler);

    // Update Constants (Ortho Projection)
    D3D11_MAPPED_SUBRESOURCE MappedCB;
    State->Context->lpVtbl->Map(State->Context, (ID3D11Resource*)State->ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedCB);
    constant_buffer *CB = (constant_buffer*)MappedCB.pData;
    
    // Ortho matrix: 2/w, 0, 0, 0 | 0, -2/h, 0, 0 | 0, 0, 1, 0 | -1, 1, 0, 1
    // HLSL is column-major by default? Wait, C side layout.
    // Standard Ortho (Top-Left 0,0, Bottom-Right W,H)
    f32 L = 0, R = WindowWidth, T = 0, B = WindowHeight;
    // X: 2/(R-L) -> 2/W
    // Y: 2/(T-B) -> 2/-H
    // Tx: -(R+L)/(R-L) -> -1
    // Ty: -(T+B)/(T-B) -> 1
    
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) CB->Projection[i][j] = 0;
    
    // Column-Major Layout for HLSL
    // Col 0: Scale X, 0, 0, Trans X
    CB->Projection[0][0] = 2.0f / WindowWidth;
    CB->Projection[0][3] = -1.0f;
    
    // Col 1: 0, Scale Y, 0, Trans Y
    CB->Projection[1][1] = -2.0f / WindowHeight;
    CB->Projection[1][3] = 1.0f;
    
    // Col 2: 0, 0, 1, 0
    CB->Projection[2][2] = 1.0f;
    
    // Col 3: 0, 0, 0, 1
    CB->Projection[3][3] = 1.0f;
    
    State->Context->lpVtbl->Unmap(State->Context, (ID3D11Resource*)State->ConstantBuffer, 0);
    State->Context->lpVtbl->VSSetConstantBuffers(State->Context, 0, 1, &State->ConstantBuffer);

    // Map VB for batching
    D3D11_MAPPED_SUBRESOURCE MappedVB;
    State->Context->lpVtbl->Map(State->Context, (ID3D11Resource*)State->VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVB);
    State->MappedVertices = (vertex*)MappedVB.pData;
    State->VertexCount = 0;
    
    // Default to White Texture
    State->CurrentTexture = State->WhiteTextureView; 
}

internal void
EndFrame(renderer_state *State)
{
    Flush(State);
    State->Context->lpVtbl->Unmap(State->Context, (ID3D11Resource*)State->VertexBuffer, 0);
    State->MappedVertices = 0;

    State->SwapChain->lpVtbl->Present(State->SwapChain, 0, 0);
}

internal void
RenderQuadUV(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, f32 U0, f32 V0, f32 U1, f32 V1, f32 Color[4])
{
    if(!State->MappedVertices || State->VertexCount + 6 > MaxVertexCount) Flush(State);
    
    // Apply Global Transform
    X += State->GlobalOffsetX;
    Y += State->GlobalOffsetY;
    f32 A = Color[3] * State->GlobalAlpha; // Use local alpha
    
    vertex *V = State->MappedVertices + State->VertexCount;
    // TL, TR, BL, BL, TR, BR
    // Params = 0 for Sharp Quad
    
    V[0].Pos[0] = X;     V[0].Pos[1] = Y;     V[0].Pos[2] = 0; V[0].UV[0] = U0; V[0].UV[1] = V0;
    V[0].Color[0] = Color[0]; V[0].Color[1] = Color[1]; V[0].Color[2] = Color[2]; V[0].Color[3] = A;
    V[0].Params[0] = 0; V[0].Params[1] = 0; V[0].Params[2] = 0;

    V[1].Pos[0] = X + W; V[1].Pos[1] = Y;     V[1].Pos[2] = 0; V[1].UV[0] = U1; V[1].UV[1] = V0;
    V[1].Color[0] = Color[0]; V[1].Color[1] = Color[1]; V[1].Color[2] = Color[2]; V[1].Color[3] = A;
    V[1].Params[0] = 0; V[1].Params[1] = 0; V[1].Params[2] = 0;

    V[2].Pos[0] = X;     V[2].Pos[1] = Y + H; V[2].Pos[2] = 0; V[2].UV[0] = U0; V[2].UV[1] = V1;
    V[2].Color[0] = Color[0]; V[2].Color[1] = Color[1]; V[2].Color[2] = Color[2]; V[2].Color[3] = A;
    V[2].Params[0] = 0; V[2].Params[1] = 0; V[2].Params[2] = 0;

    V[3] = V[2];
    V[4] = V[1];
    
    V[5].Pos[0] = X + W; V[5].Pos[1] = Y + H; V[5].Pos[2] = 0; V[5].UV[0] = U1; V[5].UV[1] = V1;
    V[5].Color[0] = Color[0]; V[5].Color[1] = Color[1]; V[5].Color[2] = Color[2]; V[5].Color[3] = A;
    V[5].Params[0] = 0; V[5].Params[1] = 0; V[5].Params[2] = 0;
    
    State->VertexCount += 6;
}

internal void
RenderRoundedQuad(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, f32 Radius, f32 Color[4])
{
     if(!State->MappedVertices || State->VertexCount + 6 > MaxVertexCount) Flush(State);
     SetTexture(State, State->WhiteTextureView); 
     
    vertex *V = State->MappedVertices + State->VertexCount;
     
     // Apply Global Transform
    X += State->GlobalOffsetX;
    Y += State->GlobalOffsetY;
    f32 A = Color[3] * State->GlobalAlpha; 
     
     // Params: W, H, Radius
     f32 P[3] = { W, H, Radius };
     
     f32 U0 = 0, V0 = 0, U1 = 1, V1 = 1;

    V[0].Pos[0] = X;     V[0].Pos[1] = Y;     V[0].Pos[2] = 0; V[0].UV[0] = U0; V[0].UV[1] = V0;
    V[0].Color[0] = Color[0]; V[0].Color[1] = Color[1]; V[0].Color[2] = Color[2]; V[0].Color[3] = A;
    V[0].Params[0] = P[0]; V[0].Params[1] = P[1]; V[0].Params[2] = P[2];

    V[1].Pos[0] = X + W; V[1].Pos[1] = Y;     V[1].Pos[2] = 0; V[1].UV[0] = U1; V[1].UV[1] = V0;
    V[1].Color[0] = Color[0]; V[1].Color[1] = Color[1]; V[1].Color[2] = Color[2]; V[1].Color[3] = A;
    V[1].Params[0] = P[0]; V[1].Params[1] = P[1]; V[1].Params[2] = P[2];

    V[2].Pos[0] = X;     V[2].Pos[1] = Y + H; V[2].Pos[2] = 0; V[2].UV[0] = U0; V[2].UV[1] = V1;
    V[2].Color[0] = Color[0]; V[2].Color[1] = Color[1]; V[2].Color[2] = Color[2]; V[2].Color[3] = A;
    V[2].Params[0] = P[0]; V[2].Params[1] = P[1]; V[2].Params[2] = P[2];

    V[3] = V[2];
    V[4] = V[1];
    
    V[5].Pos[0] = X + W; V[5].Pos[1] = Y + H; V[5].Pos[2] = 0; V[5].UV[0] = U1; V[5].UV[1] = V1;
    V[5].Color[0] = Color[0]; V[5].Color[1] = Color[1]; V[5].Color[2] = Color[2]; V[5].Color[3] = A;
    V[5].Params[0] = P[0]; V[5].Params[1] = P[1]; V[5].Params[2] = P[2];
    
    State->VertexCount += 6;
}

internal void
RenderQuad(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, f32 Color[4])
{
     SetTexture(State, State->WhiteTextureView); 
     RenderQuadUV(State, X, Y, W, H, 0, 0, 0, 0, Color); // UV 0,0 is usually white in font atlas
}

internal void
RenderQuadWithTexture(renderer_state *State, f32 X, f32 Y, f32 W, f32 H, ID3D11ShaderResourceView *Texture)
{
     SetTexture(State, Texture); 
     RenderQuadUV(State, X, Y, W, H, 0, 0, 1, 1, (f32[]){1,1,1,1});
}

internal void
ResizeD3D11(renderer_state *State, f32 Width, f32 Height)
{
    if(!State->Context) return;
    
    State->Context->lpVtbl->OMSetRenderTargets(State->Context, 0, 0, 0);
    State->BackBufferView->lpVtbl->Release(State->BackBufferView);
    
    State->SwapChain->lpVtbl->ResizeBuffers(State->SwapChain, 0, (UINT)Width, (UINT)Height, DXGI_FORMAT_UNKNOWN, 0);
    
    ID3D11Texture2D *BackBuffer;
    State->SwapChain->lpVtbl->GetBuffer(State->SwapChain, 0, &IID_ID3D11Texture2D, (void**)&BackBuffer);
    State->Device->lpVtbl->CreateRenderTargetView(State->Device, (ID3D11Resource*)BackBuffer, 0, &State->BackBufferView);
    BackBuffer->lpVtbl->Release(BackBuffer);
    
    BeginFrame(State, Width, Height); // Reset viewport
}

// WIC Loader Helper
internal ID3D11ShaderResourceView *
LoadTextureFromWIC(renderer_state *State, wchar_t *Filename)
{
    IWICImagingFactory *Factory = 0;
    IWICBitmapDecoder *Decoder = 0;
    IWICBitmapFrameDecode *Frame = 0;
    IWICFormatConverter *Converter = 0;
    
    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, 0, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void**)&Factory);
    if(FAILED(hr)) return 0;
    
    hr = Factory->lpVtbl->CreateDecoderFromFilename(Factory, Filename, 0, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &Decoder);
    if(FAILED(hr)) { Factory->lpVtbl->Release(Factory); return 0; }
    
    hr = Decoder->lpVtbl->GetFrame(Decoder, 0, &Frame);
    if(FAILED(hr)) { Decoder->lpVtbl->Release(Decoder); Factory->lpVtbl->Release(Factory); return 0; }
    
    Factory->lpVtbl->CreateFormatConverter(Factory, &Converter);
    Converter->lpVtbl->Initialize(Converter, (IWICBitmapSource*)Frame, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, 0, 0.0f, WICBitmapPaletteTypeCustom);
    
    UINT W, H;
    Converter->lpVtbl->GetSize(Converter, &W, &H);
    
    if(W > 0 && H > 0)
    {
        u32 *Pixels = (u32*)VirtualAlloc(0, W*H*4, MEM_COMMIT, PAGE_READWRITE);
        Converter->lpVtbl->CopyPixels(Converter, 0, W*4, W*H*4, (BYTE*)Pixels);
        
        D3D11_TEXTURE2D_DESC Desc = {0};
        Desc.Width = W;
        Desc.Height = H;
        Desc.MipLevels = 1;
        Desc.ArraySize = 1;
        Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_IMMUTABLE;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        D3D11_SUBRESOURCE_DATA Data = {0};
        Data.pSysMem = Pixels;
        Data.SysMemPitch = W*4;
        
        ID3D11Texture2D *Tex = 0;
        if(SUCCEEDED(State->Device->lpVtbl->CreateTexture2D(State->Device, &Desc, &Data, &Tex)))
        {
            ID3D11ShaderResourceView *View = 0;
            State->Device->lpVtbl->CreateShaderResourceView(State->Device, (ID3D11Resource*)Tex, 0, &View);
            Tex->lpVtbl->Release(Tex);
            
            VirtualFree(Pixels, 0, MEM_RELEASE);
            Converter->lpVtbl->Release(Converter);
            Frame->lpVtbl->Release(Frame);
            Decoder->lpVtbl->Release(Decoder);
            Factory->lpVtbl->Release(Factory);
            return View;
        }
        VirtualFree(Pixels, 0, MEM_RELEASE);
    }
    
    Converter->lpVtbl->Release(Converter);
    Frame->lpVtbl->Release(Frame);
    Decoder->lpVtbl->Release(Decoder);
    Factory->lpVtbl->Release(Factory);
    return 0;
}

#endif // RENDERER_C
