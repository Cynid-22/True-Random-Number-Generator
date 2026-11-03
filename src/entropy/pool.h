#pragma once
#include <array>
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
#include "entropy_common.h"
#include "source_credit.h"
#include "../crypto/hmac_drbg.h"
#include "../crypto/secure_mem.h"

namespace Entropy {

struct GenerationInput {
    EntropyDataVector newPoints;
    std::array<size_t, ENTROPY_SOURCE_COUNT> newPointCountBySource{};
    std::array<double, ENTROPY_SOURCE_COUNT> creditBitsBySource{};
    Crypto::SecureBox<Crypto::HmacDrbg> generator;
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

    void UseGenerationInput(GenerationInput& input);

    EntropyDataVector TakeAdditionalInput(GenerationInput& input,
                                          const std::set<EntropySource>& includedSources) const;

    bool HasUsedData() const;

    double GetAvailableBits(const std::set<EntropySource>& includedSources) const;

    double GetUsedBits() const;

    double GetCreditedBits(EntropySource source) const;

    double GetTotalCreditedBits() const;

    bool HealthTestFailed(EntropySource source) const;

    StartupStatus GetStartupStatus(EntropySource source) const;

    void FinishStartupEarly(EntropySource source);

    void RestartSourceStartup(EntropySource source);

    void SecureWipe();

private:
    mutable std::mutex m_mutex;
    std::array<EntropyDataVector, ENTROPY_SOURCE_COUNT> m_newDataBySource;
    std::array<SourceCreditEstimator, ENTROPY_SOURCE_COUNT> m_creditEstimators{};
    std::array<double, ENTROPY_SOURCE_COUNT> m_availableBitsBySource{};
    std::array<double, ENTROPY_SOURCE_COUNT> m_creditedBitsBySource{};
    double m_usedBits = 0.0;
    Crypto::SecureBox<Crypto::HmacDrbg> m_generator;
    uint64_t m_clearCount = 0;
};

}
