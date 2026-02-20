#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include "../entropy_common.h"

namespace Entropy {

class ClockDriftCollector {
public:
    ClockDriftCollector();
    ~ClockDriftCollector();

    // Start background collection thread
    void Start();

    // Stop background thread
    void Stop();

    // Is the collector running?
    bool IsRunning() const;
    
    // Get collected entropy (clears internal buffer and returns data)
    // Returns a vector of timestamped entropy data points
    std::vector<EntropyDataPoint> Harvest();
    
    // Statistics for GUI
    double GetEntropyRate() const;     // samples/sec estimate
    uint64_t GetSampleCount() const;   // Total samples collected
    
private:
    void CollectionLoop();
    
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
    std::atomic<uint64_t> m_sampleCount{0};
    std::atomic<double> m_rate{0.0};
    
    // Secure memory clearing helper
    void SecureClearBuffer();
};

} // namespace Entropy
