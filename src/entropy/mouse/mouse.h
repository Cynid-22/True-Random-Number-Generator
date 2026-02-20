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

    // Start background hook
    void Start();

    // Stop background hook
    void Stop();

    // Is the collector running?
    bool IsRunning() const;
    
    // Called by GUI every frame to indicate if canvas is hovered
    // Uses time-window filtering: only keeps events during hovered periods
    void SetCanvasHovered(bool hovered);

    // Get collected entropy (clears internal buffer and returns data)
    std::vector<EntropyDataPoint> Harvest();
    
    // Public method to handle mouse events (called by static hook)
    // timestamp is Entropy::GetNanosecondTimestamp()
    void ProcessMouse(POINT pt, uint64_t timestamp);
    
    // Statistics for GUI
    double GetEntropyRate() const;     // samples/sec estimate
    uint64_t GetSampleCount() const;   // Total samples collected

private:
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    void SecureClearBuffer();
    void FlushLocalBuffer();

    std::atomic<bool> m_running{false};
    HHOOK m_hook = nullptr;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;  // Events during hover periods only
    
    // Time-window filtering state
    std::atomic<bool> m_canvasHovered{false};
    std::atomic<uint64_t> m_hoverStartTime{0};  // When hover started (0 = not hovering)
    
    // State for delta calculation
    int m_lastX = -1;
    int m_lastY = -1;
    
    // Batching to prevent mutex contention at high polling rates
    static const size_t BATCH_SIZE = 128;
    std::vector<EntropyDataPoint> m_localBuffer;
    
    // Time-based flushing state
    std::atomic<uint64_t> m_lastFlushTime{0};
    
    std::atomic<uint64_t> m_sampleCount{0};
    std::atomic<double> m_rate{0.0};
    
    // Rate calculation state (avoids static locals)
    uint64_t m_lastRateTime{0};
    uint64_t m_lastRateCount{0};
    
    // Thread-local pointer for hook callback
    static MouseCollector* s_instance;
};

} // namespace Entropy

