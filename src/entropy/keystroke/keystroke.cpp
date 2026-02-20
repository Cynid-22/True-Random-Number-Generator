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
        return; // Already running
    }

    Logger::Log(Logger::Level::INFO, "Keystroke", "Installing keyboard hook...");
    
    // Reset rate tracking state for new session
    m_lastRateTime = std::chrono::steady_clock::now();
    m_sampleCount = 0;
    m_rate = 0.0;
    
    // IMPORTANT: SetWindowsHookEx requires a message loop, which we have in main.cpp
    // We install the hook here. content of module handle usually needed for global hooks, 
    // but for local thread hooks or if we are the main app properly, NULL or GetModuleHandle(NULL) 
    // is often sufficient. WH_KEYBOARD_LL is global only, so it needs module handle.
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
        return; // Not running
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

std::vector<EntropyDataPoint> KeystrokeCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }
    
    std::vector<EntropyDataPoint> harvested;
    harvested.swap(m_buffer);
    // SECURITY: Shrink to release heap block that held entropy data
    m_buffer.shrink_to_fit();
    return harvested;
}

void KeystrokeCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Crypto::SecureClearVector(m_buffer);
}

double KeystrokeCollector::GetEntropyRate() const {
    return m_rate;
}

uint64_t KeystrokeCollector::GetSampleCount() const {
    return m_sampleCount;
}

// Static callback
LRESULT CALLBACK KeystrokeCollector::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance && s_instance->IsRunning()) {
        KBDLLHOOKSTRUCT* pKbStruct = (KBDLLHOOKSTRUCT*)lParam;
        s_instance->ProcessKey(wParam, pKbStruct);
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void KeystrokeCollector::ProcessKey(WPARAM wParam, KBDLLHOOKSTRUCT* pKbStruct) {
    uint64_t now = GetNanosecondTimestamp();
    
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        OnKeyDown(now);
    } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
        OnKeyUp(now);
    }
}

void KeystrokeCollector::OnKeyDown(uint64_t timestamp) {
    // Flight time: Time since last KeyUp (if any)
    // We only calculate flight time if we have a valid previous KeyUp 
    // AND it's reasonable (e.g. < 2 seconds)
    // Actually, simple delta from last event is good enough for entropy, 
    // but the task specifically asked for "Flight Time".
    
    // For simplicity and robustness against missed events:
    // We will just capture the timestamp delta from the LAST EVENT (Down or Up)
    // inside the generic pool mixing.
    // BUT the task says: Measure Flight Time (gap between KeyUp and next KeyDown)
    
    if (m_lastKeyUpTime > 0) {
        uint64_t flightTime = 0;
        if (timestamp > m_lastKeyUpTime) {
            flightTime = timestamp - m_lastKeyUpTime;
        }
        
        // Filter: Must be < 10 seconds to be relevant "typing"
        if (flightTime > 0 && flightTime < 10000000000ULL) {
            // We have a valid flight time data point
            // Value = Flight time in nanoseconds
            
            std::lock_guard<std::mutex> lock(m_mutex);
            EntropyDataPoint pt;
            pt.timestamp = timestamp;
            pt.value = flightTime;
            pt.source = EntropySource::Keystroke;
            m_buffer.push_back(pt);
            m_sampleCount++;
            
            // Rate calc (simplified for intermittent events)
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastRateTime).count();
            if (elapsed >= 1) {
                // Just use raw count for now, it's very variable
                m_rate = (double)m_sampleCount / (double)(elapsed + 1); // rough approx
                m_lastRateTime = now;
            }
        }
    }
    
    m_lastKeyDownTime = timestamp;
}

void KeystrokeCollector::OnKeyUp(uint64_t timestamp) {
    // Dwell time: Time key was held down
    if (m_lastKeyDownTime > 0) {
        uint64_t dwellTime = 0;
        if (timestamp > m_lastKeyDownTime) {
            dwellTime = timestamp - m_lastKeyDownTime;
        }
        
        if (dwellTime > 0 && dwellTime < 2000000000ULL) { // < 2 seconds hold
             std::lock_guard<std::mutex> lock(m_mutex);
            EntropyDataPoint pt;
            pt.timestamp = timestamp;
            pt.value = dwellTime; // Dwell time is usually shorter, maybe thousands of microseconds
            pt.source = EntropySource::Keystroke;
            m_buffer.push_back(pt);
            m_sampleCount++;
        }
    }
    
    m_lastKeyUpTime = timestamp;
}

} // namespace Entropy
