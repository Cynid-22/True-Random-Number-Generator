#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <windows.h>
#include "../entropy_common.h"
#include "../raw_input_thread.h"

namespace Entropy {

class MouseCollector {
public:
    static constexpr int PACKED_X_SHIFT = 48;
    static constexpr int PACKED_Y_SHIFT = 32;
    static constexpr int PACKED_DX_SHIFT = 16;
    static constexpr uint64_t PACKED_FIELD_MASK = 0xFFFF;

    MouseCollector();
    ~MouseCollector();

    void Start();

    void Stop();

    bool IsRunning() const;

    void SetCanvasHovered(bool hovered);

    EntropyDataVector Harvest();

private:
    void ProcessInput(const RAWINPUT& input, uint64_t timestamp);
    void ProcessMovement(const RAWINPUT& input, uint64_t timestamp);
    bool IsFromPhysicalDevice(const RAWINPUTHEADER& header, uint64_t timestamp) const;
    void RecordMovement(uint64_t timestamp);
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    RawInputThread m_inputThread;
    std::mutex m_mutex;
    EntropyDataVector m_buffer;

    std::atomic<bool> m_canvasHovered{false};

    int64_t m_pendingCountsX = 0;
    int64_t m_pendingCountsY = 0;
    uint16_t m_sensorPositionX = 0;
    uint16_t m_sensorPositionY = 0;
    uint64_t m_lastSampleTime = 0;
    uint64_t m_lastTouchpadReportTime = 0;
};

}
