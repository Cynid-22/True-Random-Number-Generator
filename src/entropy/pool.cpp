#include "pool.h"
#include <algorithm>
#include <cmath>
#include <windows.h>

namespace Entropy {

float CalculateShannonEntropyBits(const ByteCounts& byteCounts) {
    uint64_t totalBytes = 0;
    for (uint64_t count : byteCounts) {
        totalBytes += count;
    }
    if (totalBytes == 0) return 0.0f;

    float entropyPerByte = 0.0f;
    float byteCount = static_cast<float>(totalBytes);

    for (uint64_t count : byteCounts) {
        float probability = static_cast<float>(count) / byteCount;
        if (probability > 0) {
            entropyPerByte -= probability * std::log2(probability);
        }
    }

    return entropyPerByte * byteCount;
}

float CalculateShannonEntropyBits(const std::vector<uint8_t>& bytes) {
    ByteCounts byteCounts{};
    for (uint8_t byte : bytes) {
        byteCounts[byte]++;
    }
    return CalculateShannonEntropyBits(byteCounts);
}

static void AddValueBytes(ByteCounts& byteCounts, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        byteCounts[(value >> (i * 8)) & 0xFF]++;
    }
}

EntropyPool::EntropyPool() {}

EntropyPool::~EntropyPool() { SecureWipe(); }

void EntropyPool::AddDataPoints(const std::vector<EntropyDataPoint>& points) {
    if (points.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& point : points) {
        const size_t sourceIndex = static_cast<size_t>(point.source);
        m_dataBySource[sourceIndex].push_back(point);
        AddValueBytes(m_newByteCountsBySource[sourceIndex], point.value);
    }
}

std::vector<EntropyDataPoint> EntropyPool::LockAndCopyDataForGeneration(
    const std::set<EntropySource>& includedSources) {
    std::vector<EntropyDataPoint> generationData;
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::array<size_t, ENTROPY_SOURCE_COUNT> lockedPointCountAfterLock =
            m_lockedPointCountBySource;
        for (EntropySource source : includedSources) {
            const size_t sourceIndex = static_cast<size_t>(source);
            lockedPointCountAfterLock[sourceIndex] = m_dataBySource[sourceIndex].size();
        }

        size_t generationPointCount = 0;
        for (size_t count : lockedPointCountAfterLock) {
            generationPointCount += count;
        }
        generationData.reserve(generationPointCount);

        for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
            const auto& sourceData = m_dataBySource[sourceIndex];
            generationData.insert(generationData.end(), sourceData.begin(),
                                  sourceData.begin() + lockedPointCountAfterLock[sourceIndex]);
        }

        for (EntropySource source : includedSources) {
            ByteCounts& newByteCounts = m_newByteCountsBySource[static_cast<size_t>(source)];
            for (size_t byteValue = 0; byteValue < newByteCounts.size(); byteValue++) {
                m_lockedByteCounts[byteValue] += newByteCounts[byteValue];
            }
            SecureZeroMemory(newByteCounts.data(), sizeof(newByteCounts));
        }
        m_lockedPointCountBySource = lockedPointCountAfterLock;
    }

    std::sort(generationData.begin(), generationData.end(),
              [](const EntropyDataPoint& a, const EntropyDataPoint& b) {
                  return a.timestamp < b.timestamp;
              });
    return generationData;
}

bool EntropyPool::IsDataLocked() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t lockedPointCount : m_lockedPointCountBySource) {
        if (lockedPointCount > 0) return true;
    }
    return false;
}

float EntropyPool::GetLockedEntropyBits() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return CalculateShannonEntropyBits(m_lockedByteCounts);
}

float EntropyPool::GetNewEntropyBits(const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    ByteCounts includedByteCounts{};
    for (EntropySource source : includedSources) {
        const ByteCounts& sourceByteCounts =
            m_newByteCountsBySource[static_cast<size_t>(source)];
        for (size_t byteValue = 0; byteValue < includedByteCounts.size(); byteValue++) {
            includedByteCounts[byteValue] += sourceByteCounts[byteValue];
        }
    }

    return CalculateShannonEntropyBits(includedByteCounts);
}

void EntropyPool::SecureWipe() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& sourceData : m_dataBySource) {
        if (!sourceData.empty()) {
            SecureZeroMemory(sourceData.data(), sourceData.size() * sizeof(EntropyDataPoint));
            sourceData.clear();
            sourceData.shrink_to_fit();
        }
    }
    m_lockedPointCountBySource.fill(0);
    WipeByteCounts();
}

void EntropyPool::WipeByteCounts() {
    SecureZeroMemory(m_lockedByteCounts.data(), sizeof(m_lockedByteCounts));
    SecureZeroMemory(m_newByteCountsBySource.data(), sizeof(m_newByteCountsBySource));
}

}
