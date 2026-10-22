#include "pool.h"
#include "../crypto/secure_mem.h"
#include <algorithm>
#include <windows.h>

namespace Entropy {

double GenerationInput::CreditBits() const {
    double bits = 0.0;
    for (double sourceBits : creditBitsBySource) {
        bits += sourceBits;
    }
    return bits;
}

void GenerationInput::SecureWipe() {
    Crypto::SecureClearVector(newPoints);
    SecureZeroMemory(carryOver.data(), carryOver.size());
    newPointCountBySource.fill(0);
    creditBitsBySource.fill(0.0);
    hasCarryOver = false;
}

EntropyPool::EntropyPool() {}

EntropyPool::~EntropyPool() { SecureWipe(); }

void EntropyPool::AddDataPoints(const EntropyDataVector& points) {
    if (points.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& point : points) {
        const size_t sourceIndex = static_cast<size_t>(point.source);
        m_newDataBySource[sourceIndex].push_back(point);
        const double creditBits =
            m_creditEstimators[sourceIndex].Credit(point.source, point.value);
        m_availableBitsBySource[sourceIndex] += creditBits;
        m_creditedBitsBySource[sourceIndex] += creditBits;
    }
}

void EntropyPool::RestartSourceTracking() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& estimator : m_creditEstimators) {
        estimator.Reset();
    }
}

GenerationInput EntropyPool::CopyGenerationInput(
    const std::set<EntropySource>& includedSources) const {
    GenerationInput input;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        size_t newPointCount = 0;
        for (EntropySource source : includedSources) {
            newPointCount += m_newDataBySource[static_cast<size_t>(source)].size();
        }
        input.newPoints.reserve(newPointCount);

        for (EntropySource source : includedSources) {
            const size_t sourceIndex = static_cast<size_t>(source);
            const auto& sourceData = m_newDataBySource[sourceIndex];
            input.newPoints.insert(input.newPoints.end(), sourceData.begin(), sourceData.end());
            input.newPointCountBySource[sourceIndex] = sourceData.size();
            input.creditBitsBySource[sourceIndex] = m_availableBitsBySource[sourceIndex];
        }

        input.carryOver = m_carryOver;
        input.hasCarryOver = m_hasCarryOver;
        input.poolClearCount = m_clearCount;
    }

    std::sort(input.newPoints.begin(), input.newPoints.end(),
              [](const EntropyDataPoint& a, const EntropyDataPoint& b) {
                  return a.timestamp < b.timestamp;
              });
    return input;
}

void EntropyPool::UseGenerationInput(const GenerationInput& input,
                                     const CarryOver& nextCarryOver) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (input.poolClearCount != m_clearCount)
        return;

    std::array<EntropyDataVector, ENTROPY_SOURCE_COUNT> unusedDataBySource;
    for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
        const auto& sourceData = m_newDataBySource[sourceIndex];
        const size_t usedCount =
            std::min(input.newPointCountBySource[sourceIndex], sourceData.size());
        if (usedCount > 0)
            unusedDataBySource[sourceIndex].assign(sourceData.begin() + usedCount,
                                                   sourceData.end());
    }

    for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
        if (input.newPointCountBySource[sourceIndex] == 0)
            continue;
        m_newDataBySource[sourceIndex].swap(unusedDataBySource[sourceIndex]);

        const double usedBits = input.creditBitsBySource[sourceIndex];
        m_availableBitsBySource[sourceIndex] =
            std::max(0.0, m_availableBitsBySource[sourceIndex] - usedBits);
        m_usedBits += usedBits;
    }

    m_carryOver = nextCarryOver;
    m_hasCarryOver = true;
}

bool EntropyPool::HasCarryOver() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hasCarryOver;
}

double EntropyPool::GetAvailableBits(const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double bits = 0.0;
    for (EntropySource source : includedSources) {
        bits += m_availableBitsBySource[static_cast<size_t>(source)];
    }
    return bits;
}

double EntropyPool::GetUsedBits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_usedBits;
}

double EntropyPool::GetCreditedBits(EntropySource source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_creditedBitsBySource[static_cast<size_t>(source)];
}

double EntropyPool::GetTotalCreditedBits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    double bits = 0.0;
    for (double sourceBits : m_creditedBitsBySource) {
        bits += sourceBits;
    }
    return bits;
}

bool EntropyPool::HealthTestFailed(EntropySource source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_creditEstimators[static_cast<size_t>(source)].HealthTestFailed();
}

StartupStatus EntropyPool::GetStartupStatus(EntropySource source) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_creditEstimators[static_cast<size_t>(source)].Startup();
}

void EntropyPool::FinishStartupEarly(EntropySource source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const size_t sourceIndex = static_cast<size_t>(source);
    const double releasedBits = m_creditEstimators[sourceIndex].FinishStartupEarly(source);
    m_availableBitsBySource[sourceIndex] += releasedBits;
    m_creditedBitsBySource[sourceIndex] += releasedBits;
}

void EntropyPool::RestartSourceStartup(EntropySource source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_creditEstimators[static_cast<size_t>(source)].Reset();
}

void EntropyPool::SecureWipe() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& sourceData : m_newDataBySource) {
        if (!sourceData.empty()) {
            SecureZeroMemory(sourceData.data(), sourceData.size() * sizeof(EntropyDataPoint));
            sourceData.clear();
            sourceData.shrink_to_fit();
        }
    }
    for (auto& estimator : m_creditEstimators) {
        estimator.Reset();
    }
    m_availableBitsBySource.fill(0.0);
    m_creditedBitsBySource.fill(0.0);
    m_usedBits = 0.0;
    SecureZeroMemory(m_carryOver.data(), m_carryOver.size());
    m_hasCarryOver = false;
    m_clearCount++;
}

}
