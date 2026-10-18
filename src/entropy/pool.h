#pragma once
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
#include "entropy_common.h"

namespace Entropy {

float CalculateShannonEntropyBits(const std::vector<uint8_t>& bytes);

class EntropyPool {
public:
    EntropyPool();
    ~EntropyPool();

    void AddDataPoints(const std::vector<EntropyDataPoint>& points);

    std::vector<EntropyDataPoint> GetPooledData() const;

    float GetEntropyBitsBefore(uint64_t timestamp) const;

    float GetEntropyBitsAfter(uint64_t timestamp, const std::set<EntropySource>& includedSources) const;

    void SecureWipe();

private:
    mutable std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_data;

    void SortByTimestamp();
};

}
