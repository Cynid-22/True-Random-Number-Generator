#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>

namespace Entropy {

enum class EntropySource {
    Microphone,
    Keystroke,
    ClockDrift,
    CpuJitter,
    Mouse
};

constexpr size_t ENTROPY_SOURCE_COUNT = static_cast<size_t>(EntropySource::Mouse) + 1;

inline uint64_t GetNanosecondTimestamp() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        high_resolution_clock::now().time_since_epoch()
    ).count();
}

struct EntropyDataPoint {
    uint64_t timestamp;
    uint64_t value;
    EntropySource source;
};

class SampleRateCounter {
public:
    void Reset() {
        m_count = 0;
        m_samplesPerSecond = 0.0;
        m_countAtWindowStart = 0;
        m_windowStart = std::chrono::steady_clock::now();
    }

    void CountSample() {
        const uint64_t count = ++m_count;
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = now - m_windowStart;
        if (elapsed >= RATE_WINDOW) {
            m_samplesPerSecond = (double)(count - m_countAtWindowStart) / elapsed.count();
            m_countAtWindowStart = count;
            m_windowStart = now;
        }
    }

    uint64_t Count() const { return m_count; }
    double PerSecond() const { return m_samplesPerSecond; }

private:
    static constexpr std::chrono::seconds RATE_WINDOW{1};

    std::atomic<uint64_t> m_count{0};
    std::atomic<double> m_samplesPerSecond{0.0};
    uint64_t m_countAtWindowStart = 0;
    std::chrono::steady_clock::time_point m_windowStart = std::chrono::steady_clock::now();
};

}
