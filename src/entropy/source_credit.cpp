#include "source_credit.h"
#include "mouse/mouse.h"
#include "../platform/virtual_machine.h"
#include <algorithm>
#include <bitset>
#include <cmath>

namespace Entropy {

namespace {

using Change = SourceCreditEstimator::Change;

struct HealthTestCutoffs {
    uint32_t repetitionCount;
    uint32_t adaptiveProportion;
};

struct SourceCreditRule {
    double maxBitsPerSample;
    size_t lowestDifferenceOrder;
    size_t highestDifferenceOrder;
};

constexpr uint32_t ADAPTIVE_PROPORTION_WINDOW = 512;
constexpr uint32_t KEYSTROKE_EARLY_STARTUP_SAMPLE_COUNT = 64;
constexpr uint32_t MOUSE_EARLY_STARTUP_SAMPLE_COUNT = 512;
constexpr int HEALTH_TEST_FALSE_ALARM_EXPONENT = 20;

constexpr size_t MICROPHONE_SAMPLES_PER_POINT = 64;
constexpr size_t MICROPHONE_ADJACENT_SAMPLE_PAIRS = MICROPHONE_SAMPLES_PER_POINT - 1;
constexpr uint64_t ADJACENT_SAMPLE_PAIRS_MASK = ~uint64_t(0) >> 1;

constexpr uint64_t SIXTEEN_BIT_MASK = 0xFFFF;
constexpr uint64_t SIXTEEN_BIT_SIGN = 0x8000;
constexpr int64_t SIXTEEN_BIT_RANGE = 0x10000;
constexpr uint64_t LARGEST_TRACKED_VALUE = uint64_t(1) << 60;

SourceCreditRule CreditRuleOf(EntropySource source) {
    switch (source) {
    case EntropySource::Microphone:
        return {8.0, 0, 0};
    case EntropySource::Keystroke:
        return {2.0, 0, 2};
    case EntropySource::ClockDrift:
        return {1.0, 1, 2};
    case EntropySource::CpuJitter:
        return {1.0, 0, 2};
    case EntropySource::Mouse:
        return {1.0, 1, 3};
    }
    return {0.0, 0, 0};
}

double BinomialProbability(uint32_t trials, uint32_t successes, double successProbability) {
    const double logProbability =
        std::lgamma(trials + 1.0) - std::lgamma(successes + 1.0) -
        std::lgamma(trials - successes + 1.0) + successes * std::log(successProbability) +
        (trials - successes) * std::log1p(-successProbability);
    return std::exp(logProbability);
}

// SP 800-90B 4.4.1 and 4.4.2, with the sample's credit cap as its min-entropy H
HealthTestCutoffs CutoffsForBitsPerSample(double bitsPerSample) {
    const double falseAlarmRate = std::ldexp(1.0, -HEALTH_TEST_FALSE_ALARM_EXPONENT);
    const double repeatProbability = std::exp2(-bitsPerSample);

    uint32_t proportionCount = ADAPTIVE_PROPORTION_WINDOW;
    double probabilityAboveCount = 0.0;
    while (proportionCount > 0) {
        const double probabilityOfCount =
            BinomialProbability(ADAPTIVE_PROPORTION_WINDOW, proportionCount, repeatProbability);
        if (probabilityAboveCount + probabilityOfCount > falseAlarmRate)
            break;
        probabilityAboveCount += probabilityOfCount;
        proportionCount--;
    }

    const uint32_t repetitionCount =
        1 + static_cast<uint32_t>(std::ceil(HEALTH_TEST_FALSE_ALARM_EXPONENT / bitsPerSample));
    return {repetitionCount, proportionCount + 1};
}

HealthTestCutoffs CutoffsOf(EntropySource source) {
    static const std::array<HealthTestCutoffs, ENTROPY_SOURCE_COUNT> cutoffsBySource = [] {
        std::array<HealthTestCutoffs, ENTROPY_SOURCE_COUNT> cutoffs{};
        for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
            const EntropySource indexedSource = static_cast<EntropySource>(sourceIndex);
            cutoffs[sourceIndex] =
                CutoffsForBitsPerSample(CreditRuleOf(indexedSource).maxBitsPerSample);
        }
        return cutoffs;
    }();
    return cutoffsBySource[static_cast<size_t>(source)];
}

bool WrapsAtSixteenBits(EntropySource source) {
    return source == EntropySource::ClockDrift || source == EntropySource::Mouse;
}

Change ValueAsPosition(EntropySource source, uint64_t value) {
    switch (source) {
    case EntropySource::Mouse:
        return {static_cast<int64_t>((value >> MouseCollector::PACKED_X_SHIFT) &
                                     MouseCollector::PACKED_FIELD_MASK),
                static_cast<int64_t>((value >> MouseCollector::PACKED_Y_SHIFT) &
                                     MouseCollector::PACKED_FIELD_MASK)};
    case EntropySource::ClockDrift:
        return {static_cast<int64_t>(value & SIXTEEN_BIT_MASK), 0};
    default:
        return {static_cast<int64_t>(std::min(value, LARGEST_TRACKED_VALUE)), 0};
    }
}

int64_t SixteenBitDifference(int64_t current, int64_t previous) {
    const uint64_t difference =
        (static_cast<uint64_t>(current) - static_cast<uint64_t>(previous)) & SIXTEEN_BIT_MASK;
    return difference >= SIXTEEN_BIT_SIGN
               ? static_cast<int64_t>(difference) - SIXTEEN_BIT_RANGE
               : static_cast<int64_t>(difference);
}

Change FirstDifference(EntropySource source, const Change& current, const Change& previous) {
    if (WrapsAtSixteenBits(source))
        return {SixteenBitDifference(current.x, previous.x),
                SixteenBitDifference(current.y, previous.y)};
    return {current.x - previous.x, current.y - previous.y};
}

Change Subtract(const Change& current, const Change& previous) {
    return {current.x - previous.x, current.y - previous.y};
}

uint64_t AbsoluteValue(int64_t value) {
    return value < 0 ? uint64_t(0) - static_cast<uint64_t>(value) : static_cast<uint64_t>(value);
}

uint64_t Magnitude(const Change& change) {
    return AbsoluteValue(change.x) + AbsoluteValue(change.y);
}

double MinEntropyPerBit(size_t matchingBits, size_t totalBits) {
    const size_t majority = std::max(matchingBits, totalBits - matchingBits);
    return -std::log2(static_cast<double>(majority) / static_cast<double>(totalBits));
}

double MicrophoneBitBalanceBits(uint64_t leastSignificantBits) {
    const size_t ones = std::bitset<64>(leastSignificantBits).count();
    const size_t bitChanges =
        std::bitset<64>((leastSignificantBits ^ (leastSignificantBits >> 1)) &
                        ADJACENT_SAMPLE_PAIRS_MASK)
            .count();
    const double bitsPerSample =
        std::min(MinEntropyPerBit(ones, MICROPHONE_SAMPLES_PER_POINT),
                 MinEntropyPerBit(bitChanges, MICROPHONE_ADJACENT_SAMPLE_PAIRS));
    return bitsPerSample * MICROPHONE_SAMPLES_PER_POINT;
}

}

double MaxCreditBitsPerSample(EntropySource source) {
    return CreditRuleOf(source).maxBitsPerSample;
}

uint32_t EarlyStartupSampleCount(EntropySource source) {
    switch (source) {
    case EntropySource::Keystroke:
        return KEYSTROKE_EARLY_STARTUP_SAMPLE_COUNT;
    case EntropySource::Mouse:
        return MOUSE_EARLY_STARTUP_SAMPLE_COUNT;
    default:
        return STARTUP_SAMPLE_COUNT;
    }
}

bool IsCreditedOnThisMachine(EntropySource source) {
    const bool reliesOnHardwareTiming =
        source == EntropySource::ClockDrift || source == EntropySource::CpuJitter;
    return !reliesOnHardwareTiming || !Platform::IsRunningInVirtualMachine();
}

double SourceCreditEstimator::Credit(EntropySource source, uint64_t value) {
    RunHealthTests(source, value);
    const bool repeatsRecentValue = RepeatsRecentValue(value);
    RememberValue(value);
    const double changeBits = (source == EntropySource::Microphone)
                                  ? MicrophoneBitBalanceBits(value)
                                  : ChangeBits(source, value);

    if (m_healthTestFailed) {
        m_heldStartupBits = 0.0;
        return 0.0;
    }
    const double sampleBits =
        (repeatsRecentValue || !IsCreditedOnThisMachine(source))
            ? 0.0
            : std::max(0.0, std::min(changeBits, MaxCreditBitsPerSample(source)));
    if (m_startupComplete)
        return sampleBits;

    m_heldStartupBits += sampleBits;
    if (++m_startupSamplesTested < STARTUP_SAMPLE_COUNT)
        return 0.0;
    return ReleaseHeldStartupBits();
}

StartupStatus SourceCreditEstimator::Startup() const {
    return {m_startupSamplesTested, m_heldStartupBits, m_startupComplete,
            m_startupFinishedEarly};
}

double SourceCreditEstimator::FinishStartupEarly(EntropySource source) {
    if (m_startupComplete || m_healthTestFailed ||
        m_startupSamplesTested < EarlyStartupSampleCount(source))
        return 0.0;
    m_startupFinishedEarly = true;
    return ReleaseHeldStartupBits();
}

double SourceCreditEstimator::ReleaseHeldStartupBits() {
    m_startupComplete = true;
    const double releasedBits = m_heldStartupBits;
    m_heldStartupBits = 0.0;
    return releasedBits;
}

void SourceCreditEstimator::Reset() {
    *this = SourceCreditEstimator{};
}

void SourceCreditEstimator::RunHealthTests(EntropySource source, uint64_t value) {
    const HealthTestCutoffs cutoffs = CutoffsOf(source);

    if (m_repetitionCount > 0 && value == m_repetitionValue) {
        if (++m_repetitionCount >= cutoffs.repetitionCount)
            m_healthTestFailed = true;
    } else {
        m_repetitionValue = value;
        m_repetitionCount = 1;
    }

    if (m_proportionWindowSamples == 0) {
        m_proportionValue = value;
        m_proportionValueCount = 1;
    } else if (value == m_proportionValue &&
               ++m_proportionValueCount >= cutoffs.adaptiveProportion) {
        m_healthTestFailed = true;
    }
    m_proportionWindowSamples = (m_proportionWindowSamples + 1) % ADAPTIVE_PROPORTION_WINDOW;
}

bool SourceCreditEstimator::RepeatsRecentValue(uint64_t value) const {
    for (size_t i = 0; i < m_recentValueCount; i++) {
        if (m_recentValues[i] == value)
            return true;
    }
    return false;
}

void SourceCreditEstimator::RememberValue(uint64_t value) {
    m_recentValues[m_nextRecentSlot] = value;
    m_nextRecentSlot = (m_nextRecentSlot + 1) % RECENT_VALUE_COUNT;
    if (m_recentValueCount < RECENT_VALUE_COUNT)
        m_recentValueCount++;
}

double SourceCreditEstimator::ChangeBits(EntropySource source, uint64_t value) {
    const SourceCreditRule rule = CreditRuleOf(source);

    std::array<Change, TRACKED_DIFFERENCE_ORDERS + 1> differences{};
    differences[0] = ValueAsPosition(source, value);
    const size_t knownOrders = std::min(m_trackedSampleCount, TRACKED_DIFFERENCE_ORDERS);
    for (size_t order = 1; order <= knownOrders; order++) {
        differences[order] =
            (order == 1) ? FirstDifference(source, differences[0], m_previousDifferences[0])
                         : Subtract(differences[order - 1], m_previousDifferences[order - 1]);
    }

    const bool hasEnoughHistory = m_trackedSampleCount >= rule.highestDifferenceOrder;
    std::copy(differences.begin(), differences.begin() + TRACKED_DIFFERENCE_ORDERS,
              m_previousDifferences.begin());
    m_trackedSampleCount = std::min(m_trackedSampleCount + 1, TRACKED_DIFFERENCE_ORDERS);
    if (!hasEnoughHistory)
        return 0.0;

    uint64_t smallestChange = UINT64_MAX;
    for (size_t order = rule.lowestDifferenceOrder; order <= rule.highestDifferenceOrder; order++)
        smallestChange = std::min(smallestChange, Magnitude(differences[order]));
    return smallestChange == 0 ? 0.0 : std::log2(static_cast<double>(smallestChange));
}

}
