#include "mouse.h"
#include "../../logging/logger.h"
#include <cmath>
#include <algorithm>
#include <windows.h> // For SecureZeroMemory

namespace Entropy {

MouseCollector* MouseCollector::s_instance = nullptr;

MouseCollector::MouseCollector() {
    s_instance = this;
    m_localBuffer.reserve(BATCH_SIZE);
    m_lastFlushTime = 0;
}

MouseCollector::~MouseCollector() {
    Stop();
    SecureClearBuffer();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void MouseCollector::Start() {
    if (m_running.exchange(true)) {
        return; // Already running
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Installing mouse hook...");

    // IMPORTANT: SetWindowsHookEx requires a message loop
    m_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    
    if (!m_hook) {
        Logger::Log(Logger::Level::ERR, "Mouse", "Failed to install mouse hook! Error: %lu", GetLastError());
        m_running = false;
        return;
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Mouse hook installed successfully.");
    
    // Reset state
    m_lastX = -1;
    m_lastY = -1;
    m_canvasHovered = false;
    m_hoverStartTime = 0;
}

void MouseCollector::Stop() {
    if (!m_running.exchange(false)) {
        return; // Not running
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Removing mouse hook...");
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }

    // Flush any remaining local buffer
    FlushLocalBuffer();

    uint64_t count = m_sampleCount.load();
    Logger::Log(Logger::Level::INFO, "Mouse", 
        "COLLECTION STOPPED | Samples: %llu | Rate: %.2f/s", 
        count, m_rate.load());
        
    SecureClearBuffer();
}

bool MouseCollector::IsRunning() const {
    return m_running;
}

void MouseCollector::SetCanvasHovered(bool hovered) {
    // Track hover state transitions for logging/debugging
    bool wasHovered = m_canvasHovered.exchange(hovered);
    
    if (hovered && !wasHovered) {
        // Hover started - record the start time
        m_hoverStartTime.store(GetNanosecondTimestamp(), std::memory_order_relaxed);
    } else if (!hovered && wasHovered) {
        // Hover ended - clear start time
        m_hoverStartTime.store(0, std::memory_order_relaxed);
    }
}

void MouseCollector::FlushLocalBuffer() {
    if (!m_localBuffer.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.insert(m_buffer.end(), m_localBuffer.begin(), m_localBuffer.end());
        m_localBuffer.clear();
    }
}

// Static callback
LRESULT CALLBACK MouseCollector::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance && s_instance->IsRunning()) {
        if (wParam == WM_MOUSEMOVE) {
            MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
            // Pass kernel event timestamp for accurate timing even with V-Sync
            s_instance->ProcessMouse(pMouseStruct->pt, pMouseStruct->time);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void MouseCollector::ProcessMouse(POINT pt, DWORD kernelTime) {
    // TIME-WINDOW FILTERING: Only capture if canvas is currently hovered
    if (!m_canvasHovered.load(std::memory_order_relaxed)) {
        return; // Canvas not hovered, skip this event
    }

    // First point initialization
    if (m_lastX == -1 || m_lastY == -1) {
        m_lastX = pt.x;
        m_lastY = pt.y;
        return;
    }

    int dx = std::abs((int)pt.x - m_lastX);
    int dy = std::abs((int)pt.y - m_lastY);

    // Filter small movements (sensor drift / noise)
    // Threshold: 2 pixels
    if (dx < 2 && dy < 2) {
        return;
    }

    // Convert kernel milliseconds to nanoseconds for entropy
    // This preserves accurate hardware timing even when processed in batches
    uint64_t eventTimeNs = (uint64_t)kernelTime * 1000000ULL;

    // Pack coordinates + deltas for high entropy density
    // Bits 48-63: X (16 bits)
    // Bits 32-47: Y (16 bits)
    // Bits 16-31: Delta X (16 bits)
    // Bits 00-15: Delta Y (16 bits)
    
    uint64_t value = ((uint64_t)(pt.x & 0xFFFF) << 48) |
                     ((uint64_t)(pt.y & 0xFFFF) << 32) |
                     ((uint64_t)(dx & 0xFFFF) << 16) |
                     ((uint64_t)(dy & 0xFFFF));

    {
        // Batch locally first to avoid mutex lock on every event
        EntropyDataPoint pt_data;
        pt_data.timestamp = eventTimeNs;  // Use kernel event time
        pt_data.value = value;
        pt_data.source = EntropySource::Mouse;
        
        m_localBuffer.push_back(pt_data);
        m_sampleCount++;

        // FLUSH CONDITION:
        // 1. Buffer full (Batching for performance)
        // 2. Time elapsed > 15ms (Latency for smoothness)
        bool shouldFlush = false;
        uint64_t now = GetNanosecondTimestamp();  // Current time for flush decision only
        if (m_localBuffer.size() >= BATCH_SIZE) {
            shouldFlush = true;
        } else {
            uint64_t last = m_lastFlushTime.load(std::memory_order_relaxed);
            // 15ms = 15,000,000 ns
            if (now > last + 15000000ULL) {
                shouldFlush = true;
            }
        }

        if (shouldFlush) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffer.insert(m_buffer.end(), m_localBuffer.begin(), m_localBuffer.end());
            m_localBuffer.clear();
            m_localBuffer.reserve(BATCH_SIZE);
            m_lastFlushTime.store(now, std::memory_order_relaxed);
        }
    }

    // Update last position
    m_lastX = pt.x;
    m_lastY = pt.y;

    // Update rate (lightweight calculation)
    static uint64_t lastRateTime = 0; 
    uint64_t timeCheck = GetNanosecondTimestamp();
    if (lastRateTime == 0) lastRateTime = timeCheck;
    
    double seconds = (double)(timeCheck - lastRateTime) / 1000000000.0;
    if (seconds >= 1.0) {
        static uint64_t lastCount = 0;
        uint64_t currentCount = m_sampleCount.load();
        m_rate = (double)(currentCount - lastCount) / seconds;
        lastCount = currentCount;
        lastRateTime = timeCheck;
    }
}

std::vector<EntropyDataPoint> MouseCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }
    
    std::vector<EntropyDataPoint> harvested;
    harvested.swap(m_buffer); 
    return harvested;
}

void MouseCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_buffer.empty()) {
        SecureZeroMemory(m_buffer.data(), m_buffer.size() * sizeof(EntropyDataPoint));
        m_buffer.clear();
    }
    if (!m_localBuffer.empty()) {
        SecureZeroMemory(m_localBuffer.data(), m_localBuffer.size() * sizeof(EntropyDataPoint));
        m_localBuffer.clear();
    }
}

double MouseCollector::GetEntropyRate() const {
    return m_rate;
}

uint64_t MouseCollector::GetSampleCount() const {
    return m_sampleCount;
}

} // namespace Entropy
