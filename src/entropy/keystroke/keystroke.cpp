#include "keystroke.h"
#include "../../logging/logger.h"
#include <hidusage.h>
#include "../../crypto/secure_mem.h"

namespace Entropy {

namespace {

constexpr USHORT FAKE_KEY_VIRTUAL_KEY = 0xFF;

bool IsOwnWindowInForeground() {
    HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return false;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(foregroundWindow, &processId);
    return processId == GetCurrentProcessId();
}

}

KeystrokeCollector::KeystrokeCollector() {
}

KeystrokeCollector::~KeystrokeCollector() {
    Stop();
    SecureClearBuffer();
}

void KeystrokeCollector::Start() {
    if (m_running.exchange(true)) {
        return;
    }

    m_keysDown.reset();
    m_lastKeyDownTime = 0;
    m_lastKeyUpTime = 0;

    Logger::Log(Logger::Level::INFO, "Keystroke", "Starting Raw Input keyboard thread...");

    const bool started = m_inputThread.Start(
        {{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD}},
        [this](const RAWINPUT& input, uint64_t timestamp) { ProcessKey(input, timestamp); },
        "Keystroke");
    if (!started) {
        m_running = false;
        return;
    }

    Logger::Log(Logger::Level::INFO, "Keystroke", "Raw Input keyboard thread started.");
}

void KeystrokeCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "Keystroke", "Stopping Raw Input keyboard thread...");
    m_inputThread.Stop();

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

size_t KeystrokeCollector::PhysicalKeyIndex(const RAWKEYBOARD& key) {
    size_t prefix = 0;
    if (key.Flags & RI_KEY_E0) {
        prefix = 1;
    } else if (key.Flags & RI_KEY_E1) {
        prefix = 2;
    }
    return prefix * SCAN_CODE_COUNT + key.MakeCode % SCAN_CODE_COUNT;
}

void KeystrokeCollector::ProcessKey(const RAWINPUT& input, uint64_t timestamp) {
    const RAWKEYBOARD& key = input.data.keyboard;
    const bool sentBySoftware = input.header.hDevice == nullptr;
    if (input.header.dwType != RIM_TYPEKEYBOARD || sentBySoftware ||
        key.VKey == FAKE_KEY_VIRTUAL_KEY || key.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE) {
        return;
    }

    const bool isKeyUp = (key.Flags & RI_KEY_BREAK) != 0;
    const size_t physicalKey = PhysicalKeyIndex(key);
    const bool isAutoRepeat = !isKeyUp && m_keysDown[physicalKey];
    m_keysDown[physicalKey] = !isKeyUp;

    if (isAutoRepeat || !IsOwnWindowInForeground()) {
        return;
    }

    if (isKeyUp) {
        OnKeyUp(timestamp);
    } else {
        OnKeyDown(timestamp);
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
