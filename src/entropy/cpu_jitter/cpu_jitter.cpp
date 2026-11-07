#include "cpu_jitter.h"
#include "../high_resolution_sleeper.h"
#include <chrono>
#include <windows.h>

namespace Entropy {

CpuJitterCollector::CpuJitterCollector() {}

CpuJitterCollector::~CpuJitterCollector() {
    Stop();
    SecureClearBuffer();
}

void CpuJitterCollector::Start() {
    if (m_running.exchange(true)) {
        return;
    }

    m_counter = 0;
    m_samples.Reset();

    m_counterThread = std::thread(&CpuJitterCollector::CounterLoop, this);
    m_samplerThread = std::thread(&CpuJitterCollector::SamplerLoop, this);
}

void CpuJitterCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    if (m_counterThread.joinable()) m_counterThread.join();
    if (m_samplerThread.joinable()) m_samplerThread.join();

    SecureClearBuffer();
}

bool CpuJitterCollector::IsRunning() const {
    return m_running;
}

void CpuJitterCollector::CounterLoop() {
    while (m_running) {
        m_counter.fetch_add(1, std::memory_order_relaxed);
    }
}

void CpuJitterCollector::SamplerLoop() {
    uint64_t previousCount = 0;

    HighResolutionSleeper sleeper;

    while (m_running) {
        sleeper.SleepFor(std::chrono::milliseconds(1));

        if (!m_running) break;

        uint64_t currentCount = m_counter.load(std::memory_order_relaxed);

        uint64_t countDelta = currentCount - previousCount;
        previousCount = currentCount;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffer.push_back({
                GetNanosecondTimestamp(),
                countDelta,
                EntropySource::CpuJitter
            });
        }
        m_samples.CountSample();
    }
}

double CpuJitterCollector::GetSamplesPerSecond() const {
    return m_samples.PerSecond();
}

uint64_t CpuJitterCollector::GetSampleCount() const {
    return m_samples.Count();
}

EntropyDataVector CpuJitterCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    EntropyDataVector harvested;
    harvested.swap(m_buffer);
    m_buffer.shrink_to_fit();
    return harvested;
}

void CpuJitterCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_buffer.empty()) {
        SecureZeroMemory(m_buffer.data(), m_buffer.size() * sizeof(EntropyDataPoint));
        m_buffer.clear();
    }
}

}
