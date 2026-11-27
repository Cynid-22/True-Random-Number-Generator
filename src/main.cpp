#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "core/app_state.h"
#include "crypto/self_test.h"
#include "gui/gui.h"
#include "gui/output/generation_task.h"
#include "logic/word_lists.h"
#include "gui/widgets.h"
#include "platform/dx11.h"
#include "platform/screen_capture.h"
#include "platform/secure_clipboard.h"
#include "platform/unicode_path.h"
#ifdef TRNG_SAMPLE_EXPORT
#include "gui/sample_export.h"
#endif

#include <climits>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <malloc.h>
#include <shellscalingapi.h>
#include <windows.h>
#pragma comment(lib, "shcore.lib")
#include "resource.h"

AppState g_state;

static constexpr SIZE_T LOCKABLE_MEMORY_BYTES = SIZE_T(128) * 1024 * 1024;
static constexpr SIZE_T WORKING_SET_MAXIMUM_BYTES = SIZE_T(1024) * 1024 * 1024;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void WipeForSessionEnd();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
        case WM_SIZE:
            if (g_d3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
                DestroyRenderTarget();
                g_swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                                           DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU)
                return 0;
            break;
        case WM_CLOSE:
            g_state.closeRequested = true;
            return 0;
        case WM_ENDSESSION:
            if (wParam)
                WipeForSessionEnd();
            return 0;
        case WM_GETMINMAXINFO: {
            LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
            minMaxInfo->ptMinTrackSize.x = 900;
            minMaxInfo->ptMinTrackSize.y = 838;
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

template <typename Collector>
static void MatchCollectorToSetting(Collector& collector, bool enabled) {
    if (enabled && !collector.IsRunning()) {
        collector.Start();
    }
    else if (!enabled && collector.IsRunning()) {
        collector.Stop();
    }
}

static void StopAllCollectors() {
    MatchCollectorToSetting(g_state.clockDriftCollector, false);
    MatchCollectorToSetting(g_state.cpuJitterCollector, false);
    MatchCollectorToSetting(g_state.keystrokeCollector, false);
    MatchCollectorToSetting(g_state.mouseCollector, false);
    MatchCollectorToSetting(g_state.microphoneCollector, false);
}

template <typename Collector>
static void HarvestSource(Collector& collector) {
    const Entropy::EntropyDataVector data = collector.Harvest();
    if (data.empty())
        return;
#ifdef TRNG_SAMPLE_EXPORT
    if (RecordSampleExportData(data))
        return;
#endif
    g_state.entropyPool.AddDataPoints(data, g_state.enabledEntropySources());
}

static void UpdateEntropyCollection() {
#ifdef TRNG_SAMPLE_EXPORT
    if (UpdateSampleExport())
        return;
#endif
    static bool wasCollecting = false;
    if (g_state.isCollecting && !wasCollecting)
        g_state.entropyPool.RestartSourceTracking();
    wasCollecting = g_state.isCollecting;

    if (g_state.isCollecting) {
        MatchCollectorToSetting(g_state.clockDriftCollector, g_state.clockDriftEnabled);
        MatchCollectorToSetting(g_state.cpuJitterCollector, g_state.cpuJitterEnabled);
        MatchCollectorToSetting(g_state.keystrokeCollector, g_state.keystrokeEnabled);
        MatchCollectorToSetting(g_state.mouseCollector, g_state.mouseMovementEnabled);
        MatchCollectorToSetting(g_state.microphoneCollector, g_state.microphoneEnabled);

        if (g_state.clockDriftEnabled)
            HarvestSource(g_state.clockDriftCollector);
        if (g_state.cpuJitterEnabled)
            HarvestSource(g_state.cpuJitterCollector);
        if (g_state.keystrokeEnabled)
            HarvestSource(g_state.keystrokeCollector);
        if (g_state.mouseMovementEnabled)
            HarvestSource(g_state.mouseCollector);
        if (g_state.microphoneEnabled)
            HarvestSource(g_state.microphoneCollector);
    } else {
        StopAllCollectors();

        g_state.mouseTrail.clear();
        Crypto::SecureClearVector(g_state.keystrokePreview);
    }
}

static void RenderTabBody(const char* id, void (*renderContents)()) {
    if (ImGui::BeginChild(id, ImVec2(0, -StatusBarReservedHeight()),
                          ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground)) {
        ImGui::Spacing();
        renderContents();
    }
    ImGui::EndChild();
}

static void RenderOutputTab() {
    RenderOutputConfigSection();
    RenderOutputSection();
}

static void RenderMainTabs() {
    static bool wasCollecting = false;
    const bool collectionJustStarted = g_state.isCollecting && !wasCollecting;
    wasCollecting = g_state.isCollecting;
    if (collectionJustStarted)
        FocusCollectionPadWhenShown();

    if (ImGui::BeginTabBar("MainTabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Input", nullptr,
                                collectionJustStarted ? ImGuiTabItemFlags_SetSelected : 0)) {
            RenderTabBody("##InputTabBody", RenderInputTab);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Output")) {
            RenderTabBody("##OutputTabBody", RenderOutputTab);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

static void RenderMainWindow(ImGuiIO& io) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("TRNG", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_MenuBar |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    CollectFinishedGeneration();
    BeginCollectionPadFrame();

    RenderMenuBar();

    RenderEntropyPoolBar();
    ImGui::Spacing();

    RenderMainTabs();

    RenderStatusBar();

    RenderAboutModal();

    RenderErrorModal();

#ifdef TRNG_SAMPLE_EXPORT
    RenderSampleExportWindow();
#endif

    RenderClosingModal();

    ImGui::End();
}

template <typename T>
static void WipeAndRelease(std::vector<T>& buffer) {
    if (!buffer.empty()) {
        SecureZeroMemory(buffer.data(), buffer.size() * sizeof(T));
        buffer.clear();
        buffer.shrink_to_fit();
    }
}

static void WipeSensitiveState() {
    g_state.entropyPool.SecureWipe();
    WipeTextInputBuffers();
    Platform::ClearSecretClipboardNow();

    Crypto::SecureClearVector(g_state.oneTimePadMessage);
    Crypto::SecureClearVector(g_state.oneTimePadFilePath);
    g_state.wipeOneTimePadResult();

    Crypto::SecureClearVector(g_state.listText);
    g_state.listItemWeights.clear();

    WipeAndRelease(g_state.generatedOutput);
    WipeAndRelease(g_state.generatedBinaryOutput);

    WipeAndRelease(g_state.keystrokePreview);

    WordLists::WipeAll();
}

static void WipeForSessionEnd() {
    g_state.cancelGenerationRequested = true;
    g_state.isCollecting = false;
    StopAllCollectors();
    g_state.entropyPool.SecureWipe();

    WipeInputTextBufferKeepingItsSize(g_state.oneTimePadMessage);
    WipeInputTextBufferKeepingItsSize(g_state.oneTimePadFilePath);
    g_state.wipeOneTimePadResult();
    WipeInputTextBufferKeepingItsSize(g_state.listText);
    g_state.listItemWeights.clear();
    g_state.listRevision++;
    g_state.wipeGeneratedOutput();
    Crypto::SecureClearVector(g_state.keystrokePreview);

    WipeTextInputBuffers();
    Platform::ClearSecretClipboardNow();
}

static void RunCryptoSelfTests() {
    const char* failedTest = Crypto::FindFailingSelfTest();
    if (!failedTest)
        return;
    g_state.selfTestFailed = true;
    ShowErrorModal("Self-test failed",
                   std::string("The cryptographic self-test \"") + failedTest +
                       "\" gave a wrong result.\nThis build of TRNG may be damaged "
                       "or miscompiled, so generating is turned off.\n\nReinstall "
                       "TRNG or build it again from the source.");
}

static void* AllocateImGuiMemory(size_t size, void*) {
    return malloc(size);
}

static void FreeImGuiMemory(void* memory, void*) {
    if (!memory)
        return;
    SecureZeroMemory(memory, _msize(memory));
    free(memory);
}

static void CopyImGuiTextAsSecret(ImGuiContext*, const char* text) {
    Platform::CopySecretTextToClipboard(text, strlen(text));
}

using FontFileData = std::vector<uint8_t>;

static std::string SystemFontsFolder() {
    wchar_t windowsFolder[MAX_PATH] = {};
    const UINT length = GetSystemWindowsDirectoryW(windowsFolder, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return std::string();
    const std::string windowsFolderUtf8 = Platform::WideToUtf8(windowsFolder);
    if (windowsFolderUtf8.empty())
        return std::string();
    return windowsFolderUtf8 + "\\Fonts\\";
}

static std::vector<FontFileData> LoadFallbackFontFiles(const std::string& fontsFolder) {
    static const char* const CJK_INDIC_AND_SYMBOL_FALLBACK_FONT_FILE_NAMES[] = {
        "msyh.ttc",
        "YuGothM.ttc",
        "malgun.ttf",
        "Nirmala.ttc",
        "Nirmala.ttf",
        "seguisym.ttf",
    };

    std::vector<FontFileData> files;
    if (fontsFolder.empty())
        return files;
    for (const char* fileName : CJK_INDIC_AND_SYMBOL_FALLBACK_FONT_FILE_NAMES) {
        const std::string path = fontsFolder + fileName;
        Platform::FileStamp stamp;
        if (!Platform::FileStampOf(path.c_str(), stamp) || stamp.size <= 0 ||
            stamp.size > INT_MAX)
            continue;
        FontFileData data;
        if (Platform::ReadWholeFileIfUnchanged(path.c_str(), stamp, data) ==
            Platform::FileReadStatus::Ok)
            files.push_back(std::move(data));
    }
    return files;
}

static void MergeFallbackFontsIntoLastFont(ImGuiIO& io, float fontSize,
                                           std::vector<FontFileData>& fallbackFonts) {
    ImFontConfig mergeConfig;
    mergeConfig.MergeMode = true;
    mergeConfig.OversampleH = 1;
    mergeConfig.OversampleV = 1;
    mergeConfig.PixelSnapH = true;
    mergeConfig.FontDataOwnedByAtlas = false;
    mergeConfig.Flags |= ImFontFlags_NoLoadError;

    for (FontFileData& data : fallbackFonts)
        io.Fonts->AddFontFromMemoryTTF(data.data(), static_cast<int>(data.size()), fontSize,
                                       &mergeConfig);
}

static ImFont* AddEmbeddedOutputFont(ImGuiIO& io, float fontSize, ImFontConfig fontConfig) {
    HRSRC resourceInfo =
        FindResourceA(nullptr, MAKEINTRESOURCEA(IDR_FONT_OUTPUT), (LPCSTR)RT_RCDATA);
    if (!resourceInfo)
        return nullptr;
    HGLOBAL resourceHandle = LoadResource(nullptr, resourceInfo);
    void* fontData = resourceHandle ? LockResource(resourceHandle) : nullptr;
    const DWORD fontDataSize = SizeofResource(nullptr, resourceInfo);
    if (!fontData || fontDataSize == 0 || fontDataSize > INT_MAX)
        return nullptr;
    fontConfig.FontDataOwnedByAtlas = false;
    fontConfig.Flags |= ImFontFlags_NoLoadError;
    return io.Fonts->AddFontFromMemoryTTF(fontData, static_cast<int>(fontDataSize), fontSize,
                                          &fontConfig);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    SetProcessWorkingSetSize(GetCurrentProcess(), LOCKABLE_MEMORY_BYTES,
                             WORKING_SET_MAXIMUM_BYTES);
    RunCryptoSelfTests();

    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    WordLists::LoadAll();

    WNDCLASSEXW windowClass = { sizeof(windowClass), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr),
                                nullptr, nullptr, nullptr, nullptr, L"TRNG", nullptr };

    windowClass.hIcon = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hIconSm = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(IDI_APP_ICON));

    RegisterClassExW(&windowClass);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int windowWidth = (int)(screenWidth * 0.7f);
    int windowHeight = (int)(screenHeight * 0.7f);
    int windowX = (screenWidth - windowWidth) / 2;
    int windowY = (screenHeight - windowHeight) / 2;

    if (windowWidth < 900) windowWidth = 900;
    if (windowHeight < 600) windowHeight = 600;

    HWND hwnd = CreateWindowW(windowClass.lpszClassName, L"TRNG - True Random Number Generator",
                              WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight,
                              nullptr, nullptr, windowClass.hInstance, nullptr);

    Platform::SetClipboardOwnerWindow(hwnd);

    if (!CreateD3DDevice(hwnd)) {
        DestroyD3DDevice();
        UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    Platform::SetCaptureProtectedWindow(hwnd);
    ApplyScreenCaptureSetting();

    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(AllocateImGuiMemory, FreeImGuiMemory, nullptr);
    ImGui::CreateContext();
    ImGui::GetPlatformIO().Platform_SetClipboardTextFn = CopyImGuiTextAsSecret;
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    SetupNativeStyle();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_d3dDevice, g_d3dDeviceContext);

    HDC deviceContext = GetDC(hwnd);
    float dpiScale = GetDeviceCaps(deviceContext, LOGPIXELSX) / 96.0f;
    ReleaseDC(hwnd, deviceContext);

    ImFontConfig fontConfig;
    fontConfig.OversampleH = 1;
    fontConfig.OversampleV = 1;
    fontConfig.PixelSnapH = true;

    float fontSize = 18.0f * dpiScale;
    const std::string fontsFolder = SystemFontsFolder();
    ImFontConfig systemFontConfig = fontConfig;
    systemFontConfig.Flags |= ImFontFlags_NoLoadError;
    ImFont* font = fontsFolder.empty()
                       ? nullptr
                       : io.Fonts->AddFontFromFileTTF((fontsFolder + "segoeui.ttf").c_str(),
                                                      fontSize, &systemFontConfig);
    if (!font) {
        io.Fonts->AddFontDefault();
    }
    std::vector<FontFileData> fallbackFontFiles = LoadFallbackFontFiles(fontsFolder);
    MergeFallbackFontsIntoLastFont(io, fontSize, fallbackFontFiles);

    ImFont* outputFont = AddEmbeddedOutputFont(io, fontSize, fontConfig);
    if (outputFont)
        MergeFallbackFontsIntoLastFont(io, fontSize, fallbackFontFiles);
    SetOutputFont(outputFont);

    ImGui::GetStyle().ScaleAllSizes(dpiScale);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        if (g_state.closeRequested && !IsGenerationRunning())
            PostQuitMessage(0);

        UpdateEntropyCollection();
        Platform::ClearSecretClipboardWhenDue();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderMainWindow(io);

        ImGui::Render();
        const float clearColor[4] = { 0.18f, 0.18f, 0.18f, 1.00f };
        g_d3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_d3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_swapChain->Present(1, 0);
    }

    WipeSensitiveState();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyD3DDevice();
    DestroyWindow(hwnd);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);

    return 0;
}
