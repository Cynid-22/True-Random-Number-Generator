#include "clock_drift.h"
#include "../../logging/logger.h"
#include <intrin.h>
#include <chrono>
#include <windows.h>

namespace Entropy {

ClockDriftCollector::ClockDriftCollector() {
}

ClockDriftCollector::~ClockDriftCollector() {
    Stop();
    SecureClearBuffer();
}

void ClockDriftCollector::Start() {
    if (m_running.exchange(true)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "ClockDrift", "Starting collector thread...");
    m_samples.Reset();
    m_thread = std::thread(&ClockDriftCollector::CollectionLoop, this);
}

void ClockDriftCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    Logger::Log(Logger::Level::INFO, "ClockDrift", "Stopping collector thread...");
    if (m_thread.joinable()) {
        m_thread.join();
    }

    Logger::Log(Logger::Level::INFO, "ClockDrift", "Collection stopped.");

    Logger::Log(Logger::Level::INFO, "ClockDrift", "Collector thread stopped.");

    SecureClearBuffer();
}

bool ClockDriftCollector::IsRunning() const {
    return m_running;
}

std::vector<EntropyDataPoint> ClockDriftCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    std::vector<EntropyDataPoint> harvested;
    harvested.swap(m_buffer);
    m_buffer.shrink_to_fit();
    return harvested;
}

void ClockDriftCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_buffer.empty()) {
        SecureZeroMemory(m_buffer.data(), m_buffer.size() * sizeof(EntropyDataPoint));
        m_buffer.clear();
        m_buffer.shrink_to_fit();
    }
}

double ClockDriftCollector::GetSamplesPerSecond() const {
    return m_samples.PerSecond();
}

uint64_t ClockDriftCollector::GetSampleCount() const {
    return m_samples.Count();
}

void ClockDriftCollector::CollectionLoop() {
    Logger::Log(Logger::Level::INFO, "ClockDrift", "Thread main loop started");

    while (m_running) {
        uint64_t cyclesBefore = __rdtsc();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        uint64_t cyclesAfter = __rdtsc();

        uint64_t cycleDelta = cyclesAfter - cyclesBefore;

        if (cycleDelta == 0 || cycleDelta > 1000000000) {
             Logger::Log(Logger::Level::WARN, "ClockDrift", "Anomalous delta detected: %llu", cycleDelta);
             continue;
        }

        uint64_t lowBits = cycleDelta & 0xFFFF;

        uint64_t timestamp = GetNanosecondTimestamp();

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            EntropyDataPoint dataPoint;
            dataPoint.timestamp = timestamp;
            dataPoint.value = lowBits;
            dataPoint.source = EntropySource::ClockDrift;
            m_buffer.push_back(dataPoint);
        }
        m_samples.CountSample();
    }
}

}
