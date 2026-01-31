#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "../entropy_common.h"

namespace Entropy {

class MicrophoneCollector {
public:
    MicrophoneCollector();
    ~MicrophoneCollector();

    // Start background capture thread
    void Start();

    // Stop background capture
    void Stop();

    // Is the collector running?
    bool IsRunning() const;

    // Get collected entropy (clears internal buffer and returns data)
    std::vector<EntropyDataPoint> Harvest();

    // Statistics for GUI
    double GetEntropyRate() const;     // samples/sec estimate
    uint64_t GetSampleCount() const;   // Total samples collected

private:
    void CaptureThread();
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    std::thread m_captureThread;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
    
    std::atomic<uint64_t> m_sampleCount{0};
    std::atomic<double> m_rate{0.0};
};

} // namespace Entropy
