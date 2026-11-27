#include "test_framework.h"
#include "test_data.h"
#include "../src/core/app_state.h"
#include "../src/entropy/pool.h"
#include "../src/entropy/source_credit.h"
#include <cstdint>

using Entropy::EntropySource;
using Entropy::SourceCreditEstimator;
using Entropy::STARTUP_SAMPLE_COUNT;
using Tests::FIRST_FULL_CREDIT_SEED;
using Tests::MICROPHONE_ONLY;

namespace {

constexpr double FULL_STARTUP_RESERVE_BITS = 12 * 512.0;
constexpr double GENERATOR_SEED_BITS = 512.0;
constexpr size_t FULL_STARTUP_RESERVE_BYTES = 768;
constexpr size_t MICROPHONE_GENERATOR_SEED_POINTS = 64;
constexpr uint32_t MICROPHONE_REPEAT_CUTOFF = 4;
constexpr uint32_t KEYSTROKE_REPEAT_CUTOFF = 11;
constexpr uint32_t CPU_JITTER_REPEAT_CUTOFF = 21;
constexpr uint64_t REPEATED_VALUE = 0x0123456789ABCDEFULL;

Entropy::EntropyPool& Pool() {
    return g_state.entropyPool;
}

void FeedFullMicrophoneStartup() {
    Pool().SecureWipe();
    Pool().AddDataPoints(Tests::SamplePoints(EntropySource::Microphone, FIRST_FULL_CREDIT_SEED, 0,
                                             STARTUP_SAMPLE_COUNT),
                         MICROPHONE_ONLY);
}

bool FailsOnlyAtTheCutoff(EntropySource source, uint32_t cutoff) {
    SourceCreditEstimator estimator;
    for (uint32_t i = 1; i < cutoff; i++) {
        estimator.Credit(source, REPEATED_VALUE);
        if (estimator.HealthTestFailed())
            return false;
    }
    estimator.Credit(source, REPEATED_VALUE);
    return estimator.HealthTestFailed();
}

double MicrophoneCreditOf(uint64_t leastSignificantBits) {
    SourceCreditEstimator estimator;
    return estimator.Credit(EntropySource::Microphone, leastSignificantBits);
}

double ThirdCredit(EntropySource source, uint64_t first, uint64_t second, uint64_t third) {
    SourceCreditEstimator estimator;
    estimator.Credit(source, first);
    estimator.Credit(source, second);
    return estimator.Credit(source, third);
}

}

TEST(CreditCapsAndEarlyStartupCounts) {
    CHECK_EQ(Entropy::MaxCreditBitsPerSample(EntropySource::Microphone), 8.0);
    CHECK_EQ(Entropy::MaxCreditBitsPerSample(EntropySource::Keystroke), 2.0);
    CHECK_EQ(Entropy::MaxCreditBitsPerSample(EntropySource::ClockDrift), 1.0);
    CHECK_EQ(Entropy::MaxCreditBitsPerSample(EntropySource::CpuJitter), 1.0);
    CHECK_EQ(Entropy::MaxCreditBitsPerSample(EntropySource::Mouse), 1.0);

    CHECK_EQ(Entropy::EarlyStartupSampleCount(EntropySource::Keystroke), 64u);
    CHECK_EQ(Entropy::EarlyStartupSampleCount(EntropySource::Mouse), 512u);
    CHECK_EQ(Entropy::EarlyStartupSampleCount(EntropySource::Microphone), STARTUP_SAMPLE_COUNT);
    CHECK_EQ(Entropy::EarlyStartupSampleCount(EntropySource::ClockDrift), STARTUP_SAMPLE_COUNT);
    CHECK_EQ(Entropy::EarlyStartupSampleCount(EntropySource::CpuJitter), STARTUP_SAMPLE_COUNT);

    CHECK(Entropy::IsCreditedOnThisMachine(EntropySource::Microphone));
    CHECK(Entropy::IsCreditedOnThisMachine(EntropySource::Keystroke));
    CHECK(Entropy::IsCreditedOnThisMachine(EntropySource::Mouse));
}

TEST(RepetitionCountCutoffsMatchTheCreditCaps) {
    CHECK(FailsOnlyAtTheCutoff(EntropySource::Microphone, MICROPHONE_REPEAT_CUTOFF));
    CHECK(FailsOnlyAtTheCutoff(EntropySource::Keystroke, KEYSTROKE_REPEAT_CUTOFF));
    CHECK(FailsOnlyAtTheCutoff(EntropySource::CpuJitter, CPU_JITTER_REPEAT_CUTOFF));
}

TEST(HealthFailureLastsUntilReset) {
    SourceCreditEstimator estimator;
    for (uint32_t i = 0; i < MICROPHONE_REPEAT_CUTOFF; i++)
        estimator.Credit(EntropySource::Microphone, REPEATED_VALUE);
    CHECK(estimator.HealthTestFailed());

    Tests::SplitMix64 random(FIRST_FULL_CREDIT_SEED);
    CHECK_EQ(estimator.Credit(EntropySource::Microphone, random.Next()), 0.0);
    CHECK(estimator.HealthTestFailed());

    estimator.Reset();
    CHECK(!estimator.HealthTestFailed());
    Tests::SplitMix64 fresh(FIRST_FULL_CREDIT_SEED);
    CHECK_EQ(estimator.Credit(EntropySource::Microphone, fresh.Next()), Tests::MICROPHONE_BITS_PER_POINT);
}

TEST(PatternedMicrophoneSamplesEarnLittleOrNoCredit) {
    CHECK_EQ(MicrophoneCreditOf(0), 0.0);
    CHECK_EQ(MicrophoneCreditOf(~uint64_t(0)), 0.0);
    CHECK_EQ(MicrophoneCreditOf(0x5555555555555555ULL), 0.0);
    const double halfAndHalf = MicrophoneCreditOf(0x00000000FFFFFFFFULL);
    CHECK(halfAndHalf > 0.0 && halfAndHalf < 2.0);
}

TEST(EvenlySpacedTimingsEarnNoCredit) {
    CHECK_EQ(ThirdCredit(EntropySource::Keystroke, 100, 200, 300), 0.0);
    CHECK_EQ(ThirdCredit(EntropySource::Keystroke, 0x1000, 0x3000, 0x7000),
             Tests::KEYSTROKE_BITS_PER_POINT);
    CHECK_EQ(ThirdCredit(EntropySource::ClockDrift, 0xFFFE, 0x0000, 0x0002), 0.0);
}

TEST(StartupCanFinishEarlyOnlyForFastSources) {
    Tests::SplitMix64 random(FIRST_FULL_CREDIT_SEED);
    const uint32_t keystrokeEarlyCount = Entropy::EarlyStartupSampleCount(EntropySource::Keystroke);

    SourceCreditEstimator keystroke;
    for (uint32_t i = 1; i < keystrokeEarlyCount; i++)
        keystroke.Credit(EntropySource::Keystroke, random.Next() >> Tests::KEYSTROKE_VALUE_SHIFT);
    CHECK(!keystroke.FinishStartupEarly(EntropySource::Keystroke));
    keystroke.Credit(EntropySource::Keystroke, random.Next() >> Tests::KEYSTROKE_VALUE_SHIFT);
    CHECK(keystroke.FinishStartupEarly(EntropySource::Keystroke));
    const Entropy::StartupStatus status = keystroke.Startup();
    CHECK(status.complete);
    CHECK(status.finishedEarly);
    CHECK_EQ(status.heldBits, 0.0);
    CHECK_EQ(status.samplesTested, keystrokeEarlyCount);
    CHECK(!keystroke.FinishStartupEarly(EntropySource::Keystroke));

    SourceCreditEstimator microphone;
    for (int i = 0; i < 100; i++)
        microphone.Credit(EntropySource::Microphone, random.Next());
    CHECK(!microphone.FinishStartupEarly(EntropySource::Microphone));

    SourceCreditEstimator failed;
    for (uint32_t i = 0; i < keystrokeEarlyCount; i++)
        failed.Credit(EntropySource::Keystroke, random.Next() >> Tests::KEYSTROKE_VALUE_SHIFT);
    for (uint32_t i = 0; i < KEYSTROKE_REPEAT_CUTOFF; i++)
        failed.Credit(EntropySource::Keystroke, REPEATED_VALUE);
    CHECK(failed.HealthTestFailed());
    CHECK(!failed.FinishStartupEarly(EntropySource::Keystroke));
}

TEST(CopyingGenerationInputChangesNothing) {
    FeedFullMicrophoneStartup();
    const Entropy::GenerationInput limited = Pool().CopyGenerationInput(MICROPHONE_ONLY, 100);
    CHECK_EQ(limited.reserveBytes.size(), size_t(100));
    CHECK_EQ(limited.newPoints.size(), MICROPHONE_GENERATOR_SEED_POINTS);
    CHECK_EQ(limited.CreditBits(), GENERATOR_SEED_BITS);
    CHECK(!limited.generator);

    const Entropy::GenerationInput everything = Pool().CopyGenerationInput(MICROPHONE_ONLY, SIZE_MAX);
    CHECK_EQ(everything.reserveBytes.size(), FULL_STARTUP_RESERVE_BYTES);

    const Entropy::GenerationInput untickedSource =
        Pool().CopyGenerationInput(Tests::KEYSTROKE_ONLY, SIZE_MAX);
    CHECK(untickedSource.reserveBytes.empty());
    CHECK(untickedSource.newPoints.empty());

    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), GENERATOR_SEED_BITS);
    CHECK_EQ(Pool().GetUsedBits(), 0.0);
}

TEST(UsingAnInputRemovesExactlyWhatItUsed) {
    FeedFullMicrophoneStartup();
    Entropy::GenerationInput input = Pool().CopyGenerationInput(MICROPHONE_ONLY, 64);
    input.reserveBytesUsed = 64;
    Pool().UseGenerationInput(input);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS - 64 * 8.0);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), 0.0);
    CHECK_EQ(Pool().GetUsedBits(), GENERATOR_SEED_BITS);
}

TEST(InputCopiedBeforeAClearIsIgnoredAfterIt) {
    FeedFullMicrophoneStartup();
    Entropy::GenerationInput staleInput = Pool().CopyGenerationInput(MICROPHONE_ONLY, 64);
    staleInput.reserveBytesUsed = 64;

    FeedFullMicrophoneStartup();
    CHECK(Pool().TakeAdditionalInput(staleInput, MICROPHONE_ONLY).empty());
    Pool().UseGenerationInput(staleInput);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), GENERATOR_SEED_BITS);
    CHECK_EQ(Pool().GetUsedBits(), 0.0);
    CHECK(!Pool().HasUsedData());
}
