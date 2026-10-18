#pragma once

#include <d3d11.h>
#include <windows.h>

extern ID3D11Device*            g_d3dDevice;
extern ID3D11DeviceContext*     g_d3dDeviceContext;
extern IDXGISwapChain*          g_swapChain;
extern ID3D11RenderTargetView*  g_mainRenderTargetView;

bool CreateD3DDevice(HWND hWnd);
void DestroyD3DDevice();
void CreateRenderTarget();
void DestroyRenderTarget();
