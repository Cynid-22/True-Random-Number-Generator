#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <windows.h>
#include "../entropy_common.h"

namespace Entropy {

class KeystrokeCollector {
public:
    KeystrokeCollector();
    ~KeystrokeCollector();

    // Start background hook
    void Start();

    // Stop background hook
    void Stop();

    // Is the collector running?
    bool IsRunning() const;
    
    // Get collected entropy (clears internal buffer and returns data)
    std::vector<EntropyDataPoint> Harvest();
    
    // Statistics for GUI
    double GetEntropyRate() const;     // samples/sec estimate
    uint64_t GetSampleCount() const;   // Total samples collected
    
    // Public method to handle key events (called by static hook)
    void ProcessKey(WPARAM wParam, KBDLLHOOKSTRUCT* pKbStruct);

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    void OnKeyDown(uint64_t timestamp);
    void OnKeyUp(uint64_t timestamp);
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    HHOOK m_hook = nullptr;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
    
    // Timing state
    uint64_t m_lastKeyDownTime = 0;
    uint64_t m_lastKeyUpTime = 0; // Not strictly needed for logic but good for tracking
    
    std::atomic<uint64_t> m_sampleCount{0};
    std::atomic<double> m_rate{0.0};
    
    // Thread-local pointer for hook callback
    static KeystrokeCollector* s_instance;
};

} // namespace Entropy
