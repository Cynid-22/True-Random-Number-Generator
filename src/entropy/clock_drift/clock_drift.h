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

    void Start();

    void Stop();

    bool IsRunning() const;

    std::vector<EntropyDataPoint> Harvest();

    double GetSamplesPerSecond() const;
    uint64_t GetSampleCount() const;

private:
    void CollectionLoop();

    std::atomic<bool> m_running{false};
    std::thread m_thread;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
    SampleRateCounter m_samples;

    void SecureClearBuffer();
};

}
