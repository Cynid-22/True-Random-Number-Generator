#include "clock_drift.h"
#include "../high_resolution_sleeper.h"
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

    m_samples.Reset();
    m_thread = std::thread(&ClockDriftCollector::CollectionLoop, this);
}

void ClockDriftCollector::Stop() {
    if (!m_running.exchange(false)) {
        return;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }

    SecureClearBuffer();
}

bool ClockDriftCollector::IsRunning() const {
    return m_running;
}

EntropyDataVector ClockDriftCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    EntropyDataVector harvested;
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
    HighResolutionSleeper sleeper;

    while (m_running) {
        uint64_t cyclesBefore = __rdtsc();

        sleeper.SleepFor(std::chrono::milliseconds(1));

        uint64_t cyclesAfter = __rdtsc();

        uint64_t cycleDelta = cyclesAfter - cyclesBefore;

        if (cycleDelta == 0 || cycleDelta > 1000000000) {
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
