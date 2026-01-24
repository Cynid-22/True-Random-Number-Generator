// TRNG - True Random Number Generator
// Main Application Entry Point
//
// File Structure:
//   src/main.cpp      - Application entry point and main loop (this file)
//   src/app_state.h   - Application state struct definition
//   src/gui.h         - GUI function declarations
//   src/gui.cpp       - GUI function implementations
//   src/dx11.h        - DirectX 11 helper declarations
//   src/dx11.cpp      - DirectX 11 helper implementations

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "core/app_state.h"
#include "gui/gui.h"
#include "logic/logic.h"
#include "platform/dx11.h"
#include "logging/logger.h"

#include <shellscalingapi.h>
#pragma comment(lib, "shcore.lib")

// Global application state
AppState g_state;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//=============================================================================
// WINDOW PROCEDURE
//=============================================================================

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), 
                                            DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_GETMINMAXINFO: {
            // Enforce minimum window size
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
            lpMMI->ptMinTrackSize.x = 900;
            lpMMI->ptMinTrackSize.y = 838;
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

//=============================================================================
// MAIN ENTRY POINT
//=============================================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Initialize Logger FIRST
    Logger::Init("logs");
    Logger::Log(Logger::Level::INFO, "Main", "Application starting...");
    
    // Enable DPI awareness for sharp rendering
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    
    // Register window class
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), 
                       nullptr, nullptr, nullptr, nullptr, L"TRNG", nullptr };
    RegisterClassExW(&wc);
    
    // Get screen size for adaptive window
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Window is 70% of screen size, centered
    int windowWidth = (int)(screenWidth * 0.7f);
    int windowHeight = (int)(screenHeight * 0.7f);
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;
    
    // Minimum size
    if (windowWidth < 900) windowWidth = 900;
    if (windowHeight < 600) windowHeight = 600;
    
    // Create application window
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"TRNG - True Random Number Generator",
                              WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight, 
                              nullptr, nullptr, wc.hInstance, nullptr);
    
    // Initialize DirectX 11
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    
    // Setup Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Disable .ini file generation
    
    // Setup native style
    SetupNativeStyle();
    
    // Initialize ImGui backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    // Get DPI scale for sharp text
    HDC hdc = GetDC(hwnd);
    float dpiScale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(hwnd, hdc);
    
    // Load font scaled by DPI
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 1;
    fontConfig.OversampleV = 1;
    fontConfig.PixelSnapH = true;
    
    float fontSize = 18.0f * dpiScale;  // Larger font for readability
    ImFont* font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", fontSize, &fontConfig);
    if (!font) {
        io.Fonts->AddFontDefault();
    }
    
    // Scale ImGui by DPI
    ImGui::GetStyle().ScaleAllSizes(dpiScale);
    
    //=========================================================================
    // MAIN LOOP
    //=========================================================================
    
    bool done = false;
    while (!done) {
        // Handle Windows messages
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;
        
        // Simulate entropy collection (placeholder)
        SimulateEntropyCollection();
        
        // Update Entropy Collection State
        if (g_state.isCollecting) {
            // Start collectors if enabled and not running
            if (g_state.clockDriftEnabled && !g_state.clockDriftCollector.IsRunning()) {
                g_state.clockDriftCollector.Start();
            }
            // Stop collectors if disabled but running (user unchecked box while collecting)
            else if (!g_state.clockDriftEnabled && g_state.clockDriftCollector.IsRunning()) {
                g_state.clockDriftCollector.Stop();
            }
            
            // Harvest Data
            if (g_state.clockDriftEnabled) {
                auto data = g_state.clockDriftCollector.Harvest();
                
                if (!data.empty()) {
                    // Update main entropy counter using the dedicated logic function
                    float newEntropy = CalculateEntropyFromDeltas(data);
                    g_state.entropyClock += newEntropy;
                    
                    // Log occasional updates
                    // Logger::Log(Logger::Level::DEBUG, "Main", "Harvested %zu samples, added %.1f bits", data.size(), newEntropy);
                }
            }

            // Simulate other sources for now until implemented
            // logic::SimulateEntropyCollection() calls are currently in GUI... 
            // We should probably move that or rely on real collectors eventually.
            // For this phase, we replace simulated Clock Drift with real one.
            
        } else {
            // Stop all collectors
            if (g_state.clockDriftCollector.IsRunning()) {
                g_state.clockDriftCollector.Stop();
            }
        }

        // Draw the GUI
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // Main window fills entire client area
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("TRNG", nullptr, 
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar);
        
        // Render menu bar
        RenderMenuBar();
        
        // Render persistent entropy pool bar at top
        RenderEntropyPoolBar();
        
        // Render tab bar for entropy sources + output
        if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("User Input")) {
                RenderUserInputTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("System Input")) {
                RenderSystemInputTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Output")) {
                ImGui::Spacing();
                RenderOutputConfigSection();
                RenderOutputSection();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        
        // Render Collection Window popup when collecting
        RenderCollectionWindow();
        
        ImGui::End();
        
        // Render frame
        ImGui::Render();
        const float clear_color[4] = { 0.18f, 0.18f, 0.18f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        g_pSwapChain->Present(1, 0);
    }
    
    //=========================================================================
    // CLEANUP
    //=========================================================================
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    
    Logger::Shutdown();
    return 0;
}
