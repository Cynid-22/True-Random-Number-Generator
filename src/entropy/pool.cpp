#include "pool.h"
#include "../../logging/logger.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <windows.h> // For SecureZeroMemory

namespace Entropy {

EntropyPool::EntropyPool() {}

EntropyPool::~EntropyPool() { SecureWipe(); }

void EntropyPool::AddDataPoint(const EntropyDataPoint &point) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_data.push_back(point);
  // Sort after adding to maintain chronological order
  EnsureSorted();
}

void EntropyPool::AddDataPoints(const std::vector<EntropyDataPoint> &points) {
  if (points.empty())
    return;

  std::lock_guard<std::mutex> lock(m_mutex);
  m_data.insert(m_data.end(), points.begin(), points.end());
  // Sort after bulk add to maintain chronological order
  EnsureSorted();
}

std::vector<EntropyDataPoint> EntropyPool::GetPooledData() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_data; // Return copy (data is already sorted)
}

std::vector<EntropyDataPoint> EntropyPool::GetPooledDataForSources(
    const std::set<EntropySource> &includedSources) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<EntropyDataPoint> filtered;
  filtered.reserve(m_data.size()); // Reserve space for efficiency

  for (const auto &point : m_data) {
    if (includedSources.find(point.source) != includedSources.end()) {
      filtered.push_back(point);
    }
  }

  // Data is already sorted chronologically, so filtered data is also sorted
  return filtered;
}

void EntropyPool::Clear() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_data.empty()) {
    // Securely zero all memory before clearing
    SecureZeroMemory(m_data.data(), m_data.size() * sizeof(EntropyDataPoint));
    m_data.clear();
    m_data.shrink_to_fit(); // Release memory
  }
}

// Helper to calculate Shannon Entropy over a byte stream
// Returns TOTAL bits of entropy in the stream
static float CalculateBytesEntropy(const std::vector<uint8_t>& data) {
    if (data.empty()) return 0.0f;
    
    std::map<uint8_t, size_t> counts;
    for (uint8_t byte : data) {
        counts[byte]++;
    }
    
    float entropyPerByte = 0.0f;
    float totalSamples = static_cast<float>(data.size());
    
    for (const auto& pair : counts) {
        float p = static_cast<float>(pair.second) / totalSamples;
        if (p > 0) {
            entropyPerByte -= p * std::log2(p);
        }
    }
    
    // Total bits = Entropy/Byte * Number of Bytes
    return entropyPerByte * totalSamples;
}

float EntropyPool::GetTotalBits() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_data.empty()) return 0.0f;

  std::vector<uint8_t> byteStream;
  byteStream.reserve(m_data.size() * 8);
  
  for (const auto& point : m_data) {
      uint64_t val = point.value;
      for (int i = 0; i < 8; i++) {
          byteStream.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
      }
  }
  
  return CalculateBytesEntropy(byteStream);
}

float EntropyPool::GetEntropyBitsBefore(uint64_t timestamp) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (timestamp == 0 || m_data.empty()) return 0.0f;

  std::vector<uint8_t> byteStream;
  byteStream.reserve(m_data.size() * 8); // Conservative reserve

  for (const auto &point : m_data) {
    if (point.timestamp <= timestamp) {
        uint64_t val = point.value;
        for (int i = 0; i < 8; i++) {
            byteStream.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    } else {
      break; // Sorted
    }
  }

  return CalculateBytesEntropy(byteStream);
}

float EntropyPool::GetEntropyBitsAfter(
    uint64_t timestamp, const std::set<EntropySource> &includedSources) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<uint8_t> byteStream;
  byteStream.reserve(m_data.size() * 8); // Conservative reserve

  for (const auto &point : m_data) {
    if (point.timestamp > timestamp) {
      if (includedSources.find(point.source) != includedSources.end()) {
          uint64_t val = point.value;
          for (int i = 0; i < 8; i++) {
              byteStream.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
          }
      }
    }
  }

  return CalculateBytesEntropy(byteStream);
}

float EntropyPool::GetTotalBits(
    uint64_t lockedTimestamp,
    const std::set<EntropySource> &includedSources) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<uint8_t> byteStream;
  byteStream.reserve(m_data.size() * 8);

  for (const auto &point : m_data) {
    bool include = false;
    
    if (point.timestamp <= lockedTimestamp) {
      include = true; // Always include locked
    } else {
      if (includedSources.find(point.source) != includedSources.end()) {
        include = true; // Include new if enabled
      }
    }
    
    if (include) {
        uint64_t val = point.value;
        for (int i = 0; i < 8; i++) {
            byteStream.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
        }
    }
  }

  return CalculateBytesEntropy(byteStream);
}

size_t EntropyPool::GetDataPointCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_data.size();
}

void EntropyPool::SecureWipe() {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_data.empty()) {
    // Securely zero all memory before clearing
    SecureZeroMemory(m_data.data(), m_data.size() * sizeof(EntropyDataPoint));
    m_data.clear();
    m_data.shrink_to_fit(); // Release memory
  }
}

void EntropyPool::EnsureSorted() {
  // Sort by timestamp to maintain chronological order
  std::sort(m_data.begin(), m_data.end(),
            [](const EntropyDataPoint &a, const EntropyDataPoint &b) {
              return a.timestamp < b.timestamp;
            });
}

} // namespace Entropy
