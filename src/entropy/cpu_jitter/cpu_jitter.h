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

    EntropyDataVector Harvest();

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

    std::mutex m_mutex;
    EntropyDataVector m_buffer;
    SampleRateCounter m_samples;
};

}
