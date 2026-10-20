#include "mouse.h"
#include "../../logging/logger.h"
#include <cmath>
#include <algorithm>
#include <windows.h>
#include "../../crypto/secure_mem.h"

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
        return;
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Installing mouse hook...");

    m_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);

    if (!m_hook) {
        Logger::Log(Logger::Level::ERR, "Mouse", "Failed to install mouse hook! Error: %lu", GetLastError());
        m_running = false;
        return;
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Mouse hook installed successfully.");

    m_lastX = -1;
    m_lastY = -1;
    m_canvasHovered = false;
}

void MouseCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Removing mouse hook...");
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }

    FlushLocalBuffer();

    Logger::Log(Logger::Level::INFO, "Mouse", "Collection stopped.");

    SecureClearBuffer();
}

bool MouseCollector::IsRunning() const {
    return m_running;
}

void MouseCollector::SetCanvasHovered(bool hovered) {
    m_canvasHovered.store(hovered);
}

void MouseCollector::FlushLocalBuffer() {
    if (!m_localBuffer.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.insert(m_buffer.end(), m_localBuffer.begin(), m_localBuffer.end());
        Crypto::SecureClearVector(m_localBuffer);
    }
}

LRESULT CALLBACK MouseCollector::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance && s_instance->IsRunning()) {
        MSLLHOOKSTRUCT* mouseEvent = (MSLLHOOKSTRUCT*)lParam;
        if (wParam == WM_MOUSEMOVE && !(mouseEvent->flags & LLMHF_INJECTED)) {
            uint64_t now = GetNanosecondTimestamp();
            s_instance->ProcessMouseMove(mouseEvent->pt, now);
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void MouseCollector::ProcessMouseMove(POINT position, uint64_t timestamp) {
    if (!m_canvasHovered.load(std::memory_order_relaxed)) {
        return;
    }

    if (m_lastX == -1 || m_lastY == -1) {
        m_lastX = position.x;
        m_lastY = position.y;
        return;
    }

    int dx = std::abs((int)position.x - m_lastX);
    int dy = std::abs((int)position.y - m_lastY);

    if (dx < 2 && dy < 2) {
        return;
    }

    uint64_t packedValue = (((uint64_t)position.x & PACKED_FIELD_MASK) << PACKED_X_SHIFT) |
                           (((uint64_t)position.y & PACKED_FIELD_MASK) << PACKED_Y_SHIFT) |
                           (((uint64_t)dx & PACKED_FIELD_MASK) << PACKED_DX_SHIFT) |
                           ((uint64_t)dy & PACKED_FIELD_MASK);

    {
        EntropyDataPoint dataPoint;
        dataPoint.timestamp = timestamp;
        dataPoint.value = packedValue;
        dataPoint.source = EntropySource::Mouse;

        m_localBuffer.push_back(dataPoint);

        bool shouldFlush = false;
        uint64_t now = GetNanosecondTimestamp();
        if (m_localBuffer.size() >= BATCH_SIZE) {
            shouldFlush = true;
        } else {
            uint64_t lastFlush = m_lastFlushTime.load(std::memory_order_relaxed);
            if (now > lastFlush + 15000000ULL) {
                shouldFlush = true;
            }
        }

        if (shouldFlush) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffer.insert(m_buffer.end(), m_localBuffer.begin(), m_localBuffer.end());
            Crypto::SecureClearVector(m_localBuffer);
            m_localBuffer.reserve(BATCH_SIZE);
            m_lastFlushTime.store(now, std::memory_order_relaxed);
        }
    }

    m_lastX = position.x;
    m_lastY = position.y;
}

EntropyDataVector MouseCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    EntropyDataVector harvested;
    harvested.swap(m_buffer);
    m_buffer.shrink_to_fit();
    return harvested;
}

void MouseCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Crypto::SecureClearVector(m_buffer);
    Crypto::SecureClearVector(m_localBuffer);
}

}
