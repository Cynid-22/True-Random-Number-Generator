#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <windows.h>
#include "../entropy_common.h"

namespace Entropy {

class MouseCollector {
public:
    MouseCollector();
    ~MouseCollector();

    void Start();

    void Stop();

    bool IsRunning() const;

    void SetCanvasHovered(bool hovered);

    std::vector<EntropyDataPoint> Harvest();

private:
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ProcessMouseMove(POINT position, uint64_t timestamp);
    void SecureClearBuffer();
    void FlushLocalBuffer();

    std::atomic<bool> m_running{false};
    HHOOK m_hook = nullptr;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;

    std::atomic<bool> m_canvasHovered{false};

    int m_lastX = -1;
    int m_lastY = -1;

    static const size_t BATCH_SIZE = 128;
    std::vector<EntropyDataPoint> m_localBuffer;

    std::atomic<uint64_t> m_lastFlushTime{0};

    static MouseCollector* s_instance;
};

}
