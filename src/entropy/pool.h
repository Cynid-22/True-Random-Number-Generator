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

constexpr double RESERVE_CHUNK_CREDIT_BITS = 640.0;
constexpr size_t RESERVE_CHUNK_MAX_POINTS = 2048;

struct GenerationInput {
    EntropyDataVector newPoints;
    std::array<size_t, ENTROPY_SOURCE_COUNT> newPointCountBySource{};
    std::array<double, ENTROPY_SOURCE_COUNT> creditBitsBySource{};
    double previouslyUsedBits = 0.0;
    Crypto::SecureBox<Crypto::HmacDrbg> generator;
    uint64_t generatorBytesSinceFullReseed = 0;
    Crypto::SecureBytes reserveBytes;
    std::array<size_t, ENTROPY_SOURCE_COUNT> reserveByteCountBySource{};
    size_t reserveBytesUsed = 0;
    double reserveBitsSeedingGenerator = 0.0;
    uint64_t poolClearCount = 0;

    double CreditBits() const;

    void SecureWipe();
};

class EntropyPool {
public:
    EntropyPool();
    ~EntropyPool();

    void AddDataPoints(const EntropyDataVector& points,
                       const std::set<EntropySource>& includedSources);

    void RestartSourceTracking();

    GenerationInput CopyGenerationInput(const std::set<EntropySource>& includedSources,
                                        size_t reserveBytesWanted) const;

    void UseGenerationInput(GenerationInput& input);

    EntropyDataVector TakeAdditionalInput(GenerationInput& input,
                                          const std::set<EntropySource>& includedSources) const;

    bool HasUsedData() const;

    double GetAvailableBits(const std::set<EntropySource>& includedSources) const;

    double GetUsedBits() const;

    double GetReserveBits(const std::set<EntropySource>& includedSources) const;

    double GetUnfinishedChunkBits(const std::set<EntropySource>& includedSources) const;

    uint64_t GetGeneratorBytesSinceFullReseed() const;

    double GetCreditedBits(EntropySource source) const;

    double GetTotalCreditedBits() const;

    bool HealthTestFailed(EntropySource source) const;

    StartupStatus GetStartupStatus(EntropySource source) const;

    void FinishStartupEarly(EntropySource source,
                            const std::set<EntropySource>& includedSources);

    void RestartSourceStartup(EntropySource source);

    void SecureWipe();

private:
    struct HeldPoint {
        EntropyDataPoint point;
        double creditBits;
    };

    using HeldPointVector = std::vector<HeldPoint, Crypto::SecureAllocator<HeldPoint>>;

    struct SourceState {
        SourceCreditEstimator estimator;
        HeldPointVector heldPoints;
        EntropyDataVector seedPoints;
        double seedBits = 0.0;
        EntropyDataVector chunkPoints;
        double chunkBits = 0.0;
        Crypto::SecureBytes reserve;
        double creditedBits = 0.0;
    };

    bool GeneratorNeedsMoreBits(const std::set<EntropySource>& includedSources) const;
    void CreditPoint(SourceState& state, const EntropyDataPoint& point, double creditBits,
                     const std::set<EntropySource>& includedSources);
    void ReleaseHeldPoints(SourceState& state, const std::set<EntropySource>& includedSources);
    static void MoveHeldPointsToSeedUncredited(SourceState& state);
    static void AddToChunk(SourceState& state, const EntropyDataPoint& point, double creditBits);
    static void CondenseChunkIntoReserve(SourceState& state);
    static void MoveChunkToSeed(SourceState& state);
    static void DiscardTaintedReserveInput(SourceState& state);
    static void WipeSourceState(SourceState& state);

    mutable std::mutex m_mutex;
    std::array<SourceState, ENTROPY_SOURCE_COUNT> m_sources;
    double m_usedBits = 0.0;
    Crypto::SecureBox<Crypto::HmacDrbg> m_generator;
    uint64_t m_generatorBytesSinceFullReseed = 0;
    uint64_t m_clearCount = 0;
};

}
