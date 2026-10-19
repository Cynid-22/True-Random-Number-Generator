#pragma once
#include <array>
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
#include "entropy_common.h"

namespace Entropy {

using ByteCounts = std::array<uint64_t, 256>;

float CalculateShannonEntropyBits(const ByteCounts& byteCounts);

float CalculateShannonEntropyBits(const std::vector<uint8_t>& bytes);

class EntropyPool {
public:
    EntropyPool();
    ~EntropyPool();

    void AddDataPoints(const std::vector<EntropyDataPoint>& points);

    std::vector<EntropyDataPoint> LockAndCopyDataForGeneration(
        const std::set<EntropySource>& includedSources);

    bool IsDataLocked() const;

    float GetLockedEntropyBits() const;

    float GetNewEntropyBits(const std::set<EntropySource>& includedSources) const;

    void SecureWipe();

private:
    mutable std::mutex m_mutex;
    std::array<std::vector<EntropyDataPoint>, ENTROPY_SOURCE_COUNT> m_dataBySource;
    std::array<size_t, ENTROPY_SOURCE_COUNT> m_lockedPointCountBySource{};
    ByteCounts m_lockedByteCounts{};
    std::array<ByteCounts, ENTROPY_SOURCE_COUNT> m_newByteCountsBySource{};

    void WipeByteCounts();
};

}
