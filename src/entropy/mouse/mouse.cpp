#include "mouse.h"
#include "../../logging/logger.h"
#include <algorithm>
#include <cstdint>
#include <hidusage.h>
#include "../../crypto/secure_mem.h"

namespace Entropy {

namespace {

// Just under a standard 125 Hz mouse's 8 ms, so a faster mouse can't earn more credit
constexpr uint64_t MINIMUM_SAMPLE_INTERVAL_NS = 7000000ULL;

constexpr USHORT TOUCH_PAD_USAGE = 0x05;
// A precision touchpad reports at 125 Hz or more while touched
constexpr uint64_t TOUCHPAD_TOUCH_TIMEOUT_NS = 50000000ULL;

int16_t ClampToSixteenBits(int64_t count) {
    return static_cast<int16_t>(std::clamp<int64_t>(count, INT16_MIN, INT16_MAX));
}

uint64_t PackedField(uint16_t field, int shift) {
    return static_cast<uint64_t>(field) << shift;
}

}

MouseCollector::MouseCollector() {
}

MouseCollector::~MouseCollector() {
    Stop();
    SecureClearBuffer();
}

void MouseCollector::Start() {
    if (m_running.exchange(true)) {
        return;
    }

    m_canvasHovered = false;
    m_pendingCountsX = 0;
    m_pendingCountsY = 0;
    m_sensorPositionX = 0;
    m_sensorPositionY = 0;
    m_lastSampleTime = 0;
    m_lastTouchpadReportTime = 0;

    Logger::Log(Logger::Level::INFO, "Mouse", "Starting Raw Input mouse thread...");

    const bool started = m_inputThread.Start(
        {{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE},
         {HID_USAGE_PAGE_DIGITIZER, TOUCH_PAD_USAGE}},
        [this](const RAWINPUT& input, uint64_t timestamp) { ProcessInput(input, timestamp); },
        "Mouse");
    if (!started) {
        m_running = false;
        return;
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Raw Input mouse thread started.");
}

void MouseCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "Mouse", "Stopping Raw Input mouse thread...");
    m_inputThread.Stop();

    Logger::Log(Logger::Level::INFO, "Mouse", "Collection stopped.");

    SecureClearBuffer();
}

bool MouseCollector::IsRunning() const {
    return m_running;
}

void MouseCollector::SetCanvasHovered(bool hovered) {
    m_canvasHovered.store(hovered);
}

void MouseCollector::ProcessInput(const RAWINPUT& input, uint64_t timestamp) {
    if (input.header.dwType == RIM_TYPEMOUSE) {
        ProcessMovement(input, timestamp);
    } else if (input.header.dwType == RIM_TYPEHID && input.header.hDevice != nullptr) {
        m_lastTouchpadReportTime = timestamp;
    }
}

bool MouseCollector::IsFromPhysicalDevice(const RAWINPUTHEADER& header, uint64_t timestamp) const {
    // A precision touchpad's movement has no device handle, like movement sent by software
    const bool touchpadIsTouched = timestamp - m_lastTouchpadReportTime < TOUCHPAD_TOUCH_TIMEOUT_NS;
    return header.hDevice != nullptr || touchpadIsTouched;
}

void MouseCollector::ProcessMovement(const RAWINPUT& input, uint64_t timestamp) {
    const RAWMOUSE& mouse = input.data.mouse;
    const bool isRelativeMovement = !(mouse.usFlags & MOUSE_MOVE_ABSOLUTE) &&
                                    (mouse.lLastX != 0 || mouse.lLastY != 0);
    if (!isRelativeMovement || !IsFromPhysicalDevice(input.header, timestamp)) {
        return;
    }

    if (!m_canvasHovered.load(std::memory_order_relaxed)) {
        m_pendingCountsX = 0;
        m_pendingCountsY = 0;
        return;
    }

    m_pendingCountsX += mouse.lLastX;
    m_pendingCountsY += mouse.lLastY;
    if (timestamp - m_lastSampleTime >= MINIMUM_SAMPLE_INTERVAL_NS) {
        RecordMovement(timestamp);
    }
}

void MouseCollector::RecordMovement(uint64_t timestamp) {
    const int16_t dx = ClampToSixteenBits(m_pendingCountsX);
    const int16_t dy = ClampToSixteenBits(m_pendingCountsY);
    m_sensorPositionX = static_cast<uint16_t>(m_sensorPositionX + dx);
    m_sensorPositionY = static_cast<uint16_t>(m_sensorPositionY + dy);
    m_pendingCountsX = 0;
    m_pendingCountsY = 0;
    m_lastSampleTime = timestamp;

    EntropyDataPoint dataPoint;
    dataPoint.timestamp = timestamp;
    dataPoint.value = PackedField(m_sensorPositionX, PACKED_X_SHIFT) |
                      PackedField(m_sensorPositionY, PACKED_Y_SHIFT) |
                      PackedField(static_cast<uint16_t>(dx), PACKED_DX_SHIFT) |
                      static_cast<uint16_t>(dy);
    dataPoint.source = EntropySource::Mouse;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.push_back(dataPoint);
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
}

}
