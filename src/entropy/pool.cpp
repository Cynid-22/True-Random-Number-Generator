#include "pool.h"
#include "../../config/AppConfig.h"
#include "../crypto/secure_mem.h"
#include "../crypto/toeplitz.h"
#include <algorithm>
#include <utility>

namespace Entropy {

static constexpr size_t BITS_PER_BYTE = 8;

static_assert(RESERVE_CHUNK_MAX_POINTS * sizeof(uint64_t) <=
                  Crypto::ToeplitzExtractor::MAX_INPUT_BYTES,
              "a full chunk must fit the extractor's input");

double GenerationInput::CreditBits() const {
    double bits = 0.0;
    for (double sourceBits : creditBitsBySource) {
        bits += sourceBits;
    }
    return bits;
}

void GenerationInput::SecureWipe() {
    Crypto::SecureClearVector(newPoints);
    newPointCountBySource.fill(0);
    creditBitsBySource.fill(0.0);
    previouslyUsedBits = 0.0;
    generator.Reset();
    generatorBytesSinceFullReseed = 0;
    Crypto::SecureClearVector(reserveBytes);
    reserveByteCountBySource.fill(0);
    reserveBytesUsed = 0;
    reserveBitsSeedingGenerator = 0.0;
}

template <typename Vector>
static void WipeAndRelease(Vector& data) {
    Crypto::SecureClearVector(data);
    data.shrink_to_fit();
}

template <typename Vector>
static Vector WithoutFront(const Vector& data, size_t count) {
    return Vector(data.begin() + std::min(count, data.size()), data.end());
}

static void AppendBigEndian(Crypto::SecureBytes& bytes, uint64_t value) {
    for (size_t i = sizeof(value); i-- > 0;) {
        bytes.push_back(static_cast<uint8_t>(value >> (i * BITS_PER_BYTE)));
    }
}

static void SortByTimestamp(EntropyDataVector& points) {
    std::sort(points.begin(), points.end(),
              [](const EntropyDataPoint& a, const EntropyDataPoint& b) {
                  return a.timestamp < b.timestamp;
              });
}

EntropyPool::EntropyPool() {}

EntropyPool::~EntropyPool() { SecureWipe(); }

void EntropyPool::AddDataPoints(const EntropyDataVector& points,
                                const std::set<EntropySource>& includedSources) {
    if (points.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& point : points) {
        SourceState& state = m_sources[static_cast<size_t>(point.source)];
        const bool wasFailing = state.estimator.HealthTestFailed();
        const bool wasInStartupTest = !state.estimator.Startup().complete;
        const double creditBits = state.estimator.Credit(point.source, point.value);

        if (state.estimator.HealthTestFailed()) {
            if (!wasFailing)
                DiscardTaintedReserveInput(state);
            state.seedPoints.push_back(point);
        } else if (wasInStartupTest) {
            state.heldPoints.push_back({point, creditBits});
            if (state.estimator.Startup().complete)
                ReleaseHeldPoints(state, includedSources);
        } else {
            CreditPoint(state, point, creditBits, includedSources);
        }
    }
}

bool EntropyPool::GeneratorNeedsMoreBits(const std::set<EntropySource>& includedSources) const {
    double generatorBits = m_usedBits;
    for (EntropySource source : includedSources) {
        generatorBits += m_sources[static_cast<size_t>(source)].seedBits;
    }
    return generatorBits < AppConfig::MINIMUM_ENTROPY_BITS;
}

void EntropyPool::CreditPoint(SourceState& state, const EntropyDataPoint& point,
                              double creditBits,
                              const std::set<EntropySource>& includedSources) {
    state.creditedBits += creditBits;
    if (!IsCreditedOnThisMachine(point.source) || GeneratorNeedsMoreBits(includedSources)) {
        state.seedPoints.push_back(point);
        state.seedBits += creditBits;
    } else {
        AddToChunk(state, point, creditBits);
    }
}

void EntropyPool::ReleaseHeldPoints(SourceState& state,
                                    const std::set<EntropySource>& includedSources) {
    for (const HeldPoint& held : state.heldPoints) {
        CreditPoint(state, held.point, held.creditBits, includedSources);
    }
    WipeAndRelease(state.heldPoints);
}

void EntropyPool::MoveHeldPointsToSeedUncredited(SourceState& state) {
    for (const HeldPoint& held : state.heldPoints) {
        state.seedPoints.push_back(held.point);
    }
    WipeAndRelease(state.heldPoints);
}

void EntropyPool::AddToChunk(SourceState& state, const EntropyDataPoint& point,
                             double creditBits) {
    state.chunkPoints.push_back(point);
    state.chunkBits += creditBits;
    if (state.chunkBits >= RESERVE_CHUNK_CREDIT_BITS)
        CondenseChunkIntoReserve(state);
    else if (state.chunkPoints.size() >= RESERVE_CHUNK_MAX_POINTS)
        MoveChunkToSeed(state);
}

void EntropyPool::CondenseChunkIntoReserve(SourceState& state) {
    Crypto::SecureBytes chunkInput;
    chunkInput.reserve(state.chunkPoints.size() * sizeof(uint64_t));
    for (const auto& point : state.chunkPoints) {
        AppendBigEndian(chunkInput, point.value);
    }

    const size_t reserveSize = state.reserve.size();
    state.reserve.resize(reserveSize + Crypto::ToeplitzExtractor::OUTPUT_BYTES);
    if (!Crypto::ToeplitzExtractor::Extract(chunkInput.data(), chunkInput.size(),
                                            state.reserve.data() + reserveSize))
        state.reserve.resize(reserveSize);

    Crypto::SecureClearVector(state.chunkPoints);
    state.chunkBits = 0.0;
}

void EntropyPool::MoveChunkToSeed(SourceState& state) {
    state.seedPoints.insert(state.seedPoints.end(), state.chunkPoints.begin(),
                            state.chunkPoints.end());
    state.seedBits += state.chunkBits;
    Crypto::SecureClearVector(state.chunkPoints);
    state.chunkBits = 0.0;
}

void EntropyPool::DiscardTaintedReserveInput(SourceState& state) {
    Crypto::SecureClearVector(state.chunkPoints);
    state.chunkBits = 0.0;
    MoveHeldPointsToSeedUncredited(state);
}

void EntropyPool::RestartSourceTracking() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& state : m_sources) {
        MoveHeldPointsToSeedUncredited(state);
        state.estimator.Reset();
    }
}

GenerationInput EntropyPool::CopyGenerationInput(
    const std::set<EntropySource>& includedSources, size_t reserveBytesWanted) const {
    GenerationInput input;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        size_t newPointCount = 0;
        size_t reserveByteCount = 0;
        for (EntropySource source : includedSources) {
            const SourceState& state = m_sources[static_cast<size_t>(source)];
            newPointCount += state.seedPoints.size();
            reserveByteCount += state.reserve.size();
        }
        input.newPoints.reserve(newPointCount);
        input.reserveBytes.reserve(std::min(reserveByteCount, reserveBytesWanted));

        for (EntropySource source : includedSources) {
            const size_t sourceIndex = static_cast<size_t>(source);
            const SourceState& state = m_sources[sourceIndex];
            input.newPoints.insert(input.newPoints.end(), state.seedPoints.begin(),
                                   state.seedPoints.end());
            input.newPointCountBySource[sourceIndex] = state.seedPoints.size();
            input.creditBitsBySource[sourceIndex] = state.seedBits;

            const size_t reserveBytesCopied =
                std::min(state.reserve.size(), reserveBytesWanted - input.reserveBytes.size());
            input.reserveBytes.insert(input.reserveBytes.end(), state.reserve.begin(),
                                      state.reserve.begin() + reserveBytesCopied);
            input.reserveByteCountBySource[sourceIndex] = reserveBytesCopied;
        }

        input.previouslyUsedBits = m_usedBits;
        input.generator = m_generator;
        input.generatorBytesSinceFullReseed = m_generatorBytesSinceFullReseed;
        input.poolClearCount = m_clearCount;
    }

    SortByTimestamp(input.newPoints);
    return input;
}

EntropyDataVector EntropyPool::TakeAdditionalInput(
    GenerationInput& input, const std::set<EntropySource>& includedSources) const {
    EntropyDataVector points;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (input.poolClearCount != m_clearCount)
            return points;

        for (EntropySource source : includedSources) {
            const size_t sourceIndex = static_cast<size_t>(source);
            const SourceState& state = m_sources[sourceIndex];
            const size_t absorbedCount =
                std::min(input.newPointCountBySource[sourceIndex], state.seedPoints.size());
            points.insert(points.end(), state.seedPoints.begin() + absorbedCount,
                          state.seedPoints.end());
            input.newPointCountBySource[sourceIndex] = state.seedPoints.size();
            input.creditBitsBySource[sourceIndex] =
                std::max(input.creditBitsBySource[sourceIndex], state.seedBits);
        }
    }

    SortByTimestamp(points);
    return points;
}

void EntropyPool::UseGenerationInput(GenerationInput& input) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (input.poolClearCount != m_clearCount)
        return;

    std::array<EntropyDataVector, ENTROPY_SOURCE_COUNT> unusedSeedPointsBySource;
    std::array<Crypto::SecureBytes, ENTROPY_SOURCE_COUNT> unusedReserveBySource;
    std::array<size_t, ENTROPY_SOURCE_COUNT> usedReserveBytesBySource{};
    size_t reserveBytesLeftToRemove = input.reserveBytesUsed;
    for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
        const SourceState& state = m_sources[sourceIndex];
        if (input.newPointCountBySource[sourceIndex] > 0)
            unusedSeedPointsBySource[sourceIndex] =
                WithoutFront(state.seedPoints, input.newPointCountBySource[sourceIndex]);

        usedReserveBytesBySource[sourceIndex] = std::min(
            reserveBytesLeftToRemove, input.reserveByteCountBySource[sourceIndex]);
        reserveBytesLeftToRemove -= usedReserveBytesBySource[sourceIndex];
        if (usedReserveBytesBySource[sourceIndex] > 0)
            unusedReserveBySource[sourceIndex] =
                WithoutFront(state.reserve, usedReserveBytesBySource[sourceIndex]);
    }

    for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
        SourceState& state = m_sources[sourceIndex];
        if (usedReserveBytesBySource[sourceIndex] > 0)
            state.reserve.swap(unusedReserveBySource[sourceIndex]);
        if (input.newPointCountBySource[sourceIndex] == 0)
            continue;
        state.seedPoints.swap(unusedSeedPointsBySource[sourceIndex]);

        const double usedBits = input.creditBitsBySource[sourceIndex];
        state.seedBits = std::max(0.0, state.seedBits - usedBits);
        m_usedBits += usedBits;
    }

    m_usedBits += input.reserveBitsSeedingGenerator;
    m_generator = std::move(input.generator);
    m_generatorBytesSinceFullReseed = input.generatorBytesSinceFullReseed;
}

bool EntropyPool::HasUsedData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<bool>(m_generator);
}

double EntropyPool::GetAvailableBits(const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double bits = 0.0;
    for (EntropySource source : includedSources) {
        bits += m_sources[static_cast<size_t>(source)].seedBits;
    }
    return bits;
}

double EntropyPool::GetUsedBits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_usedBits;
}

double EntropyPool::GetReserveBits(const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t reserveBytes = 0;
    for (EntropySource source : includedSources) {
        reserveBytes += m_sources[static_cast<size_t>(source)].reserve.size();
    }
    return static_cast<double>(reserveBytes) * BITS_PER_BYTE;
}

double EntropyPool::GetUnfinishedChunkBits(const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double bits = 0.0;
    for (EntropySource source : includedSources) {
        bits += m_sources[static_cast<size_t>(source)].chunkBits;
    }
    return bits;
}

uint64_t EntropyPool::GetGeneratorBytesSinceFullReseed() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generatorBytesSinceFullReseed;
}

double EntropyPool::GetCreditedBits(EntropySource source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sources[static_cast<size_t>(source)].creditedBits;
}

double EntropyPool::GetTotalCreditedBits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double bits = 0.0;
    for (const SourceState& state : m_sources) {
        bits += state.creditedBits;
    }
    return bits;
}

bool EntropyPool::HealthTestFailed(EntropySource source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sources[static_cast<size_t>(source)].estimator.HealthTestFailed();
}

StartupStatus EntropyPool::GetStartupStatus(EntropySource source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sources[static_cast<size_t>(source)].estimator.Startup();
}

void EntropyPool::FinishStartupEarly(EntropySource source,
                                     const std::set<EntropySource>& includedSources) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SourceState& state = m_sources[static_cast<size_t>(source)];
    if (state.estimator.FinishStartupEarly(source))
        ReleaseHeldPoints(state, includedSources);
}

void EntropyPool::RestartSourceStartup(EntropySource source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    SourceState& state = m_sources[static_cast<size_t>(source)];
    MoveHeldPointsToSeedUncredited(state);
    state.estimator.Reset();
}

void EntropyPool::WipeSourceState(SourceState& state) {
    WipeAndRelease(state.heldPoints);
    WipeAndRelease(state.seedPoints);
    WipeAndRelease(state.chunkPoints);
    WipeAndRelease(state.reserve);
    state.seedBits = 0.0;
    state.chunkBits = 0.0;
    state.creditedBits = 0.0;
    state.estimator.Reset();
}

void EntropyPool::SecureWipe() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& state : m_sources) {
        WipeSourceState(state);
    }
    m_usedBits = 0.0;
    m_generator.Reset();
    m_generatorBytesSinceFullReseed = 0;
    m_clearCount++;
}

}
