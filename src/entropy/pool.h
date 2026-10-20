#pragma once
#include <array>
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
#include "entropy_common.h"
#include "source_credit.h"

namespace Entropy {

constexpr size_t CARRY_OVER_BYTES = 64;
using CarryOver = std::array<uint8_t, CARRY_OVER_BYTES>;

struct GenerationInput {
    EntropyDataVector newPoints;
    std::array<size_t, ENTROPY_SOURCE_COUNT> newPointCountBySource{};
    std::array<double, ENTROPY_SOURCE_COUNT> creditBitsBySource{};
    CarryOver carryOver{};
    bool hasCarryOver = false;
    uint64_t poolClearCount = 0;

    double CreditBits() const;

    void SecureWipe();
};

class EntropyPool {
public:
    EntropyPool();
    ~EntropyPool();

    void AddDataPoints(const EntropyDataVector& points);

    void RestartSourceTracking();

    GenerationInput CopyGenerationInput(const std::set<EntropySource>& includedSources) const;

    void UseGenerationInput(const GenerationInput& input, const CarryOver& nextCarryOver);

    bool HasCarryOver() const;

    double GetAvailableBits(const std::set<EntropySource>& includedSources) const;

    double GetUsedBits() const;

    double GetCreditedBits(EntropySource source) const;

    double GetTotalCreditedBits() const;

    bool HealthTestFailed(EntropySource source) const;

    void SecureWipe();

private:
    mutable std::mutex m_mutex;
    std::array<EntropyDataVector, ENTROPY_SOURCE_COUNT> m_newDataBySource;
    std::array<SourceCreditEstimator, ENTROPY_SOURCE_COUNT> m_creditEstimators{};
    std::array<double, ENTROPY_SOURCE_COUNT> m_availableBitsBySource{};
    std::array<double, ENTROPY_SOURCE_COUNT> m_creditedBitsBySource{};
    double m_usedBits = 0.0;
    CarryOver m_carryOver{};
    bool m_hasCarryOver = false;
    uint64_t m_clearCount = 0;
};

}
