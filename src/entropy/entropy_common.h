#pragma once
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

}
