#pragma once
#include <cstdint>
#include <chrono>

namespace Entropy {

// Entropy source types
enum class EntropySource {
    Microphone,
    Keystroke,
    ClockDrift,
    CpuJitter,
    Mouse
};

// High-precision timestamp (nanoseconds since epoch)
inline uint64_t GetNanosecondTimestamp() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Data point with timestamp and source
struct EntropyDataPoint {
    uint64_t timestamp;     // Nanosecond timestamp
    uint64_t value;         // Collected entropy value (raw delta, etc)
    EntropySource source;  // Which source generated this data point
};

} // namespace Entropy
