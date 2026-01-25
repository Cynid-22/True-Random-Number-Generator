#pragma once
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
#include "entropy_common.h"

namespace Entropy {

// Centralized entropy pool for collecting and managing timestamped entropy data
// All data stays in memory only - never written to disk
class EntropyPool {
public:
    EntropyPool();
    ~EntropyPool();

    // Add a single data point to the pool
    void AddDataPoint(const EntropyDataPoint& point);

    // Bulk add data points from a collector
    void AddDataPoints(const std::vector<EntropyDataPoint>& points);

    // Get all pooled data sorted chronologically
    std::vector<EntropyDataPoint> GetPooledData() const;

    // Get pooled data filtered by included source types
    std::vector<EntropyDataPoint> GetPooledDataForSources(const std::set<EntropySource>& includedSources) const;

    // Clear pool (for new collection session) - securely wipes memory
    void Clear();

    // Calculate total entropy bits from pooled data (conservative estimate)
    float GetTotalBits() const;

    // Get count of data points in pool
    size_t GetDataPointCount() const;

    // SECURITY: Securely wipe all data and zero memory (called on shutdown)
    void SecureWipe();

private:
    // Internal storage - sorted chronologically by timestamp
    mutable std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_data;

    // Helper to ensure data is sorted chronologically
    void EnsureSorted();
};

} // namespace Entropy
