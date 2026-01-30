#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include "../entropy_common.h"

namespace Entropy {

class CpuJitterCollector {
public:
    CpuJitterCollector();
    ~CpuJitterCollector();

    // Start background collection threads
    void Start();

    // Stop background threads
    void Stop();

    // Is the collector running?
    bool IsRunning() const;

    // Get collected entropy (clears internal buffer and returns data)
    std::vector<EntropyDataPoint> Harvest();

    // Statistics for GUI
    double GetEntropyRate() const;
    uint64_t GetSampleCount() const;

private:
    void RunnerLoop();
    void RefereeLoop();
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    std::thread m_runnerThread;
    std::thread m_refereeThread;
    
    // Counter for runner
    std::atomic<uint64_t> m_counter{0};
    
    // Pause flag for precise snapshot
    std::atomic<bool> m_paused{false};

    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;

    std::atomic<uint64_t> m_sampleCount{0};
    std::atomic<double> m_rate{0.0};
};

} // namespace Entropy
