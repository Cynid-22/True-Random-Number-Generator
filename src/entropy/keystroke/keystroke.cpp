#include "keystroke.h"
#include "../../logging/logger.h"
#include <windows.h>
#include <chrono>
#include "../../crypto/secure_mem.h"

namespace Entropy {

KeystrokeCollector* KeystrokeCollector::s_instance = nullptr;

KeystrokeCollector::KeystrokeCollector() {
    s_instance = this;
}

KeystrokeCollector::~KeystrokeCollector() {
    Stop();
    SecureClearBuffer();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void KeystrokeCollector::Start() {
    if (m_running.exchange(true)) {
        return;
    }

    m_keysDown.reset();
    m_lastKeyDownTime = 0;
    m_lastKeyUpTime = 0;

    Logger::Log(Logger::Level::INFO, "Keystroke", "Installing keyboard hook...");

    m_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);

    if (!m_hook) {
        Logger::Log(Logger::Level::ERR, "Keystroke", "Failed to install keyboard hook! Error: %lu", GetLastError());
        m_running = false;
        return;
    }

    Logger::Log(Logger::Level::INFO, "Keystroke", "Keyboard hook installed successfully.");
}

void KeystrokeCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "Keystroke", "Removing keyboard hook...");
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }

    Logger::Log(Logger::Level::INFO, "Keystroke", "Collection stopped.");

    SecureClearBuffer();
}

bool KeystrokeCollector::IsRunning() const {
    return m_running;
}

EntropyDataVector KeystrokeCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    EntropyDataVector harvested;
    harvested.swap(m_buffer);
    m_buffer.shrink_to_fit();
    return harvested;
}

void KeystrokeCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Crypto::SecureClearVector(m_buffer);
}

static bool IsOwnWindowInForeground() {
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return false;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(foregroundWindow, &processId);
    return processId == GetCurrentProcessId();
}

LRESULT CALLBACK KeystrokeCollector::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance && s_instance->IsRunning()) {
        s_instance->ProcessKeyMessage(wParam, *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam));
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void KeystrokeCollector::ProcessKeyMessage(WPARAM message, const KBDLLHOOKSTRUCT& keyEvent) {
    uint64_t now = GetNanosecondTimestamp();

    if (keyEvent.flags & LLKHF_INJECTED) {
        return;
    }

    const bool isKeyDown = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
    const bool isKeyUp = (message == WM_KEYUP || message == WM_SYSKEYUP);
    const size_t virtualKey = keyEvent.vkCode % VIRTUAL_KEY_COUNT;
    const bool isAutoRepeat = isKeyDown && m_keysDown[virtualKey];
    if (isKeyDown) {
        m_keysDown[virtualKey] = true;
    } else if (isKeyUp) {
        m_keysDown[virtualKey] = false;
    }

    if (isAutoRepeat || !IsOwnWindowInForeground()) {
        return;
    }

    if (isKeyDown) {
        OnKeyDown(now);
    } else if (isKeyUp) {
        OnKeyUp(now);
    }
}

void KeystrokeCollector::OnKeyDown(uint64_t timestamp) {
    if (m_lastKeyUpTime > 0) {
        uint64_t flightTime = 0;
        if (timestamp > m_lastKeyUpTime) {
            flightTime = timestamp - m_lastKeyUpTime;
        }

        if (flightTime > 0 && flightTime < 10000000000ULL) {
            std::lock_guard<std::mutex> lock(m_mutex);
            EntropyDataPoint dataPoint;
            dataPoint.timestamp = timestamp;
            dataPoint.value = flightTime;
            dataPoint.source = EntropySource::Keystroke;
            m_buffer.push_back(dataPoint);
        }
    }

    m_lastKeyDownTime = timestamp;
}

void KeystrokeCollector::OnKeyUp(uint64_t timestamp) {
    if (m_lastKeyDownTime > 0) {
        uint64_t dwellTime = 0;
        if (timestamp > m_lastKeyDownTime) {
            dwellTime = timestamp - m_lastKeyDownTime;
        }

        if (dwellTime > 0 && dwellTime < 2000000000ULL) {
            std::lock_guard<std::mutex> lock(m_mutex);
            EntropyDataPoint dataPoint;
            dataPoint.timestamp = timestamp;
            dataPoint.value = dwellTime;
            dataPoint.source = EntropySource::Keystroke;
            m_buffer.push_back(dataPoint);
        }
    }

    m_lastKeyUpTime = timestamp;
}

}
