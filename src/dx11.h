// TRNG - DirectX 11 Helpers Header
// Contains DirectX initialization and cleanup functions

#pragma once

#include <d3d11.h>
#include <windows.h>

//=============================================================================
// DIRECTX 11 GLOBALS
//=============================================================================
extern ID3D11Device*            g_pd3dDevice;
extern ID3D11DeviceContext*     g_pd3dDeviceContext;
extern IDXGISwapChain*          g_pSwapChain;
extern ID3D11RenderTargetView*  g_mainRenderTargetView;

//=============================================================================
// DIRECTX 11 FUNCTIONS
//=============================================================================
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
