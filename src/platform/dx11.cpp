#include "dx11.h"

ID3D11Device*            g_d3dDevice = nullptr;
ID3D11DeviceContext*     g_d3dDeviceContext = nullptr;
IDXGISwapChain*          g_swapChain = nullptr;
ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

using CreateDeviceAndSwapChainFunction = decltype(&D3D11CreateDeviceAndSwapChain);

static CreateDeviceAndSwapChainFunction LoadCreateDeviceAndSwapChain() {
    HMODULE d3d11 = LoadLibraryExW(L"d3d11.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!d3d11)
        return nullptr;
    return reinterpret_cast<CreateDeviceAndSwapChainFunction>(
        GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain"));
}

bool CreateD3DDevice(HWND hWnd) {
    const CreateDeviceAndSwapChainFunction createDeviceAndSwapChain =
        LoadCreateDeviceAndSwapChain();
    if (!createDeviceAndSwapChain)
        return false;

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = 0;
    swapChainDesc.BufferDesc.Height = 0;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT result = createDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                              createDeviceFlags, featureLevels, 2,
                                              D3D11_SDK_VERSION, &swapChainDesc, &g_swapChain,
                                              &g_d3dDevice, &featureLevel, &g_d3dDeviceContext);
    if (result == DXGI_ERROR_UNSUPPORTED)
        result = createDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                          createDeviceFlags, featureLevels, 2,
                                          D3D11_SDK_VERSION, &swapChainDesc, &g_swapChain,
                                          &g_d3dDevice, &featureLevel, &g_d3dDeviceContext);
    if (result != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void DestroyD3DDevice() {
    DestroyRenderTarget();
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_d3dDeviceContext) { g_d3dDeviceContext->Release(); g_d3dDeviceContext = nullptr; }
    if (g_d3dDevice) { g_d3dDevice->Release(); g_d3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* backBuffer;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
    backBuffer->Release();
}

void DestroyRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
