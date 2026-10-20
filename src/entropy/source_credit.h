#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "entropy_common.h"

namespace Entropy {

double MaxCreditBitsPerSample(EntropySource source);

class SourceCreditEstimator {
public:
    double Credit(EntropySource source, uint64_t value);

    bool HealthTestFailed() const { return m_healthTestFailed; }

    void Reset();

    struct Change {
        int64_t x;
        int64_t y;
    };

private:
    static constexpr size_t RECENT_VALUE_COUNT = 8;
    static constexpr size_t TRACKED_DIFFERENCE_ORDERS = 3;

    void RunHealthTests(EntropySource source, uint64_t value);
    bool RepeatsRecentValue(uint64_t value) const;
    void RememberValue(uint64_t value);
    double ChangeBits(EntropySource source, uint64_t value);

    bool m_healthTestFailed = false;

    uint64_t m_repetitionValue = 0;
    uint32_t m_repetitionCount = 0;

    uint64_t m_proportionValue = 0;
    uint32_t m_proportionValueCount = 0;
    uint32_t m_proportionWindowSamples = 0;

    std::array<uint64_t, RECENT_VALUE_COUNT> m_recentValues{};
    size_t m_recentValueCount = 0;
    size_t m_nextRecentSlot = 0;

    std::array<Change, TRACKED_DIFFERENCE_ORDERS> m_previousDifferences{};
    size_t m_trackedSampleCount = 0;
};

}
