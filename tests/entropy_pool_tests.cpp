#include "test_framework.h"
#include "test_data.h"
#include "../src/core/app_state.h"
#include "../src/entropy/pool.h"
#include "../src/entropy/source_credit.h"

using Entropy::EntropySource;
using Entropy::SourceCreditEstimator;
using Entropy::STARTUP_SAMPLE_COUNT;
using Tests::FIRST_SAMPLE_SEED;
using Tests::MICROPHONE_BITS_PER_POINT;
using Tests::MICROPHONE_ONLY;
using Tests::SamplePoints;
using Tests::SECOND_SAMPLE_SEED;

namespace {

constexpr double GENERATOR_SEED_BITS = 512.0;
constexpr double RESERVE_BITS_PER_CHUNK = 512.0;
constexpr size_t MICROPHONE_POINTS_PER_CHUNK = 80;
constexpr size_t MICROPHONE_GENERATOR_SEED_POINTS = 64;
constexpr size_t PARTIAL_CHUNK_POINTS = 40;
constexpr size_t CONSTANT_VALUE_REPEATS = 32;
constexpr uint64_t CONSTANT_VALUE = 0x0123456789ABCDEFULL;
constexpr size_t HELD_POINTS = 100;

const double FULL_STARTUP_RESERVE_BITS =
    static_cast<double>((STARTUP_SAMPLE_COUNT - MICROPHONE_GENERATOR_SEED_POINTS) /
                        MICROPHONE_POINTS_PER_CHUNK) *
    RESERVE_BITS_PER_CHUNK;

Entropy::EntropyPool& Pool() {
    return g_state.entropyPool;
}

void FeedFullMicrophoneStartup() {
    Pool().SecureWipe();
    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, FIRST_SAMPLE_SEED, 0,
                                      STARTUP_SAMPLE_COUNT),
                         MICROPHONE_ONLY);
}

}

TEST(RepeatedRecentValuesEarnNoCredit) {
    SourceCreditEstimator estimator;
    Tests::SplitMix64 random(FIRST_SAMPLE_SEED);
    const uint64_t first = random.Next();
    const uint64_t second = random.Next();
    CHECK_EQ(estimator.Credit(EntropySource::Microphone, first), MICROPHONE_BITS_PER_POINT);
    CHECK_EQ(estimator.Credit(EntropySource::Microphone, second), MICROPHONE_BITS_PER_POINT);
    CHECK_EQ(estimator.Credit(EntropySource::Microphone, first), 0.0);
}

TEST(KeystrokeCreditNeedsHistoryAndIsCapped) {
    SourceCreditEstimator estimator;
    Tests::SplitMix64 random(FIRST_SAMPLE_SEED);
    auto nextValue = [&random] { return random.Next() >> Tests::KEYSTROKE_VALUE_SHIFT; };
    CHECK_EQ(estimator.Credit(EntropySource::Keystroke, nextValue()), 0.0);
    CHECK_EQ(estimator.Credit(EntropySource::Keystroke, nextValue()), 0.0);
    CHECK_EQ(estimator.Credit(EntropySource::Keystroke, nextValue()),
             Tests::KEYSTROKE_BITS_PER_POINT);
}

TEST(ConstantInputFailsTheHealthTest) {
    SourceCreditEstimator estimator;
    for (size_t i = 0; i < CONSTANT_VALUE_REPEATS; i++)
        estimator.Credit(EntropySource::Microphone, CONSTANT_VALUE);
    CHECK(estimator.HealthTestFailed());
    CHECK_EQ(estimator.Startup().heldBits, 0.0);
}

TEST(StartupTestHoldsCreditUntilItPasses) {
    Pool().SecureWipe();
    Pool().AddDataPoints(
        SamplePoints(EntropySource::Microphone, FIRST_SAMPLE_SEED, 0, HELD_POINTS),
        MICROPHONE_ONLY);

    const Entropy::StartupStatus during = Pool().GetStartupStatus(EntropySource::Microphone);
    CHECK_EQ(during.samplesTested, static_cast<uint32_t>(HELD_POINTS));
    CHECK_EQ(during.heldBits, HELD_POINTS * MICROPHONE_BITS_PER_POINT);
    CHECK(!during.complete);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetTotalCreditedBits(), 0.0);

    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, FIRST_SAMPLE_SEED,
                                      HELD_POINTS, STARTUP_SAMPLE_COUNT - HELD_POINTS),
                         MICROPHONE_ONLY);
    CHECK(Pool().GetStartupStatus(EntropySource::Microphone).complete);
    CHECK_EQ(Pool().GetTotalCreditedBits(), STARTUP_SAMPLE_COUNT * MICROPHONE_BITS_PER_POINT);
}

TEST(RestartingTheStartupTestReleasesNoCredit) {
    Pool().SecureWipe();
    Pool().AddDataPoints(
        SamplePoints(EntropySource::Microphone, FIRST_SAMPLE_SEED, 0, HELD_POINTS),
        MICROPHONE_ONLY);
    Pool().RestartSourceStartup(EntropySource::Microphone);

    const Entropy::StartupStatus restarted = Pool().GetStartupStatus(EntropySource::Microphone);
    CHECK_EQ(restarted.samplesTested, 0u);
    CHECK_EQ(restarted.heldBits, 0.0);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), 0.0);
}

TEST(GeneratorSeedComesBeforeTheReserve) {
    FeedFullMicrophoneStartup();
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), GENERATOR_SEED_BITS);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetUsedBits(), 0.0);
    CHECK(!Pool().HasUsedData());
}

TEST(UntickedSourcesAreLeftOut) {
    FeedFullMicrophoneStartup();
    CHECK_EQ(Pool().GetAvailableBits(Tests::KEYSTROKE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(Tests::KEYSTROKE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(Tests::MICROPHONE_AND_KEYSTROKE), FULL_STARTUP_RESERVE_BITS);
}

TEST(PartialChunkWaitsOutsideTheReserve) {
    FeedFullMicrophoneStartup();
    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, SECOND_SAMPLE_SEED, 0,
                                      PARTIAL_CHUNK_POINTS),
                         MICROPHONE_ONLY);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY),
             PARTIAL_CHUNK_POINTS * MICROPHONE_BITS_PER_POINT);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);

    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, SECOND_SAMPLE_SEED,
                                      PARTIAL_CHUNK_POINTS,
                                      MICROPHONE_POINTS_PER_CHUNK - PARTIAL_CHUNK_POINTS),
                         MICROPHONE_ONLY);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY),
             FULL_STARTUP_RESERVE_BITS + RESERVE_BITS_PER_CHUNK);
}

TEST(FailingSourceDropsItsChunk) {
    FeedFullMicrophoneStartup();
    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, SECOND_SAMPLE_SEED, 0,
                                      PARTIAL_CHUNK_POINTS),
                         MICROPHONE_ONLY);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY),
             PARTIAL_CHUNK_POINTS * MICROPHONE_BITS_PER_POINT);

    Entropy::EntropyDataVector constantPoints;
    for (size_t i = 0; i < CONSTANT_VALUE_REPEATS; i++)
        constantPoints.push_back({i + 1, CONSTANT_VALUE, EntropySource::Microphone});
    Pool().AddDataPoints(constantPoints, MICROPHONE_ONLY);

    CHECK(Pool().HealthTestFailed(EntropySource::Microphone));
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);

    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, SECOND_SAMPLE_SEED,
                                      PARTIAL_CHUNK_POINTS, 2 * MICROPHONE_POINTS_PER_CHUNK),
                         MICROPHONE_ONLY);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), GENERATOR_SEED_BITS);
}

TEST(ClearWipesTheWholePool) {
    FeedFullMicrophoneStartup();
    Pool().SecureWipe();
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetTotalCreditedBits(), 0.0);
    CHECK_EQ(Pool().GetUsedBits(), 0.0);
    CHECK_EQ(Pool().GetStartupStatus(EntropySource::Microphone).samplesTested, 0u);
    CHECK(!Pool().HasUsedData());
}
