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

    void Start();

    void Stop();

    bool IsRunning() const;

    std::vector<EntropyDataPoint> Harvest();

    double GetSamplesPerSecond() const;
    uint64_t GetSampleCount() const;

private:
    void CounterLoop();
    void SamplerLoop();
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    std::thread m_counterThread;
    std::thread m_samplerThread;

    std::atomic<uint64_t> m_counter{0};

    std::atomic<bool> m_counterPaused{false};

    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
    SampleRateCounter m_samples;
};

}
