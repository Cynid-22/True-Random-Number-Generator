#include "cpu_jitter.h"
#include "../../logging/logger.h"
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

    Logger::Log(Logger::Level::INFO, "CpuJitter", "Starting collector threads...");

    m_counterPaused = false;
    m_counter = 0;
    m_samples.Reset();

    m_counterThread = std::thread(&CpuJitterCollector::CounterLoop, this);
    m_samplerThread = std::thread(&CpuJitterCollector::SamplerLoop, this);
}

void CpuJitterCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "CpuJitter", "Stopping collector threads...");

    if (m_counterThread.joinable()) m_counterThread.join();
    if (m_samplerThread.joinable()) m_samplerThread.join();

    Logger::Log(Logger::Level::INFO, "CpuJitter", "Collection stopped.");
}

bool CpuJitterCollector::IsRunning() const {
    return m_running;
}

void CpuJitterCollector::CounterLoop() {
    while (m_running) {
        if (!m_counterPaused) {
            m_counter.fetch_add(1, std::memory_order_relaxed);
        } else {
            std::this_thread::yield();
        }
    }
}

void CpuJitterCollector::SamplerLoop() {
    Logger::Log(Logger::Level::INFO, "CpuJitter", "Referee thread started (Race Condition Active)");
    uint64_t previousCount = 0;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        if (!m_running) break;

        m_counterPaused = true;

        uint64_t currentCount = m_counter.load(std::memory_order_relaxed);

        m_counterPaused = false;

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

std::vector<EntropyDataPoint> CpuJitterCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<EntropyDataPoint> harvested;
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
