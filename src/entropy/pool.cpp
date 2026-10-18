#include "pool.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <windows.h>

namespace Entropy {

float CalculateShannonEntropyBits(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) return 0.0f;

    std::map<uint8_t, size_t> counts;
    for (uint8_t byte : bytes) {
        counts[byte]++;
    }

    float entropyPerByte = 0.0f;
    float byteCount = static_cast<float>(bytes.size());

    for (const auto& pair : counts) {
        float probability = static_cast<float>(pair.second) / byteCount;
        if (probability > 0) {
            entropyPerByte -= probability * std::log2(probability);
        }
    }

    return entropyPerByte * byteCount;
}

static void AppendValueBytes(std::vector<uint8_t>& byteStream, uint64_t value) {
    for (int i = 0; i < 8; i++) {
        byteStream.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

EntropyPool::EntropyPool() {}

EntropyPool::~EntropyPool() { SecureWipe(); }

void EntropyPool::AddDataPoints(const std::vector<EntropyDataPoint>& points) {
    if (points.empty())
        return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.insert(m_data.end(), points.begin(), points.end());
    SortByTimestamp();
}

std::vector<EntropyDataPoint> EntropyPool::GetPooledData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data;
}

float EntropyPool::GetEntropyBitsBefore(uint64_t timestamp) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (timestamp == 0 || m_data.empty()) return 0.0f;

    std::vector<uint8_t> byteStream;
    byteStream.reserve(m_data.size() * 8);

    for (const auto& point : m_data) {
        if (point.timestamp <= timestamp) {
            AppendValueBytes(byteStream, point.value);
        } else {
            break;
        }
    }

    return CalculateShannonEntropyBits(byteStream);
}

float EntropyPool::GetEntropyBitsAfter(
    uint64_t timestamp, const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint8_t> byteStream;
    byteStream.reserve(m_data.size() * 8);

    for (const auto& point : m_data) {
        if (point.timestamp > timestamp &&
            includedSources.find(point.source) != includedSources.end()) {
            AppendValueBytes(byteStream, point.value);
        }
    }

    return CalculateShannonEntropyBits(byteStream);
}

void EntropyPool::SecureWipe() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_data.empty()) {
        SecureZeroMemory(m_data.data(), m_data.size() * sizeof(EntropyDataPoint));
        m_data.clear();
        m_data.shrink_to_fit();
    }
}

void EntropyPool::SortByTimestamp() {
    std::sort(m_data.begin(), m_data.end(),
              [](const EntropyDataPoint& a, const EntropyDataPoint& b) {
                  return a.timestamp < b.timestamp;
              });
}

}
