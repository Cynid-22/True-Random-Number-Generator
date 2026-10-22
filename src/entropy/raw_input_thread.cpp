#include "raw_input_thread.h"
#include "entropy_common.h"
#include "../logging/logger.h"
#include <algorithm>
#include <utility>

namespace Entropy {

namespace {

constexpr const wchar_t* MESSAGE_WINDOW_CLASS = L"TRNGRawInput";
constexpr DWORD RECEIVE_ONLY_WHILE_IN_FOREGROUND = 0;
constexpr UINT RAW_INPUT_READ_FAILED = static_cast<UINT>(-1);

HWND CreateMessageWindow() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = MESSAGE_WINDOW_CLASS;
    if (!RegisterClassExW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return nullptr;
    return CreateWindowExW(0, MESSAGE_WINDOW_CLASS, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                           windowClass.hInstance, nullptr);
}

bool RegisterDevices(std::vector<RAWINPUTDEVICE>& registrations) {
    return RegisterRawInputDevices(registrations.data(), static_cast<UINT>(registrations.size()),
                                   sizeof(RAWINPUTDEVICE));
}

}

RawInputThread::~RawInputThread() {
    Stop();
}

bool RawInputThread::Start(std::vector<HidUsage> devices, InputHandler handler,
                           const char* logModule) {
    m_handler = std::move(handler);
    m_logModule = logModule;

    std::promise<bool> started;
    std::future<bool> startResult = started.get_future();
    m_thread = std::thread(&RawInputThread::Run, this, std::move(devices), std::move(started));
    if (startResult.get())
        return true;

    m_thread.join();
    return false;
}

void RawInputThread::Stop() {
    if (!m_thread.joinable())
        return;
    PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    m_thread.join();
}

void RawInputThread::Run(std::vector<HidUsage> devices, std::promise<bool> started) {
    // Each input is timestamped when this thread wakes, so it must wake ahead of busy threads
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    HWND window = CreateMessageWindow();
    std::vector<RAWINPUTDEVICE> registrations;
    for (const HidUsage& device : devices)
        registrations.push_back({device.page, device.usage, RECEIVE_ONLY_WHILE_IN_FOREGROUND, window});
    if (!window || !RegisterDevices(registrations)) {
        Logger::Log(Logger::Level::ERR, m_logModule, "Failed to register for Raw Input! Error: %lu",
                    GetLastError());
        if (window)
            DestroyWindow(window);
        started.set_value(false);
        return;
    }
    m_threadId = GetCurrentThreadId();
    started.set_value(true);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_INPUT)
            DeliverInput(reinterpret_cast<HRAWINPUT>(message.lParam));
        DispatchMessageW(&message);
    }

    for (RAWINPUTDEVICE& registration : registrations) {
        registration.dwFlags = RIDEV_REMOVE;
        registration.hwndTarget = nullptr;
    }
    RegisterDevices(registrations);
    DestroyWindow(window);
}

void RawInputThread::DeliverInput(HRAWINPUT rawInput) {
    const uint64_t timestamp = GetNanosecondTimestamp();
    UINT size = 0;
    if (GetRawInputData(rawInput, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0)
        return;
    const size_t bufferSize = std::max<size_t>(size, sizeof(RAWINPUT));
    if (m_inputBuffer.size() < bufferSize)
        m_inputBuffer.resize(bufferSize);

    if (GetRawInputData(rawInput, RID_INPUT, m_inputBuffer.data(), &size, sizeof(RAWINPUTHEADER)) !=
        RAW_INPUT_READ_FAILED)
        m_handler(*reinterpret_cast<const RAWINPUT*>(m_inputBuffer.data()), timestamp);
    SecureZeroMemory(m_inputBuffer.data(), m_inputBuffer.size());
}

}
