#include "pool.h"
#include "../../logging/logger.h"
#include <algorithm>
#include <cmath>
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

float EntropyPool::GetTotalBits() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_data.empty())
    return 0.0f;

  // Conservative estimate: ~2 bits per data point
  // This matches the estimate used in CalculateEntropyFromDeltas
  return static_cast<float>(m_data.size()) * 2.0f;
}

float EntropyPool::GetEntropyBitsBefore(uint64_t timestamp) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  // If no lock (timestamp 0), nothing is locked
  if (timestamp == 0)
    return 0.0f;

  size_t count = 0;
  for (const auto &point : m_data) {
    if (point.timestamp <= timestamp) {
      count++;
    } else {
      // Since data is sorted, we can break early
      break;
    }
  }

  return static_cast<float>(count) * 2.0f;
}

float EntropyPool::GetEntropyBitsAfter(
    uint64_t timestamp, const std::set<EntropySource> &includedSources) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  size_t count = 0;
  for (const auto &point : m_data) {
    // Only count data AFTER the timestamp
    if (point.timestamp > timestamp) {
      // AND check if source is included
      if (includedSources.find(point.source) != includedSources.end()) {
        count++;
      }
    }
  }

  return static_cast<float>(count) * 2.0f;
}

float EntropyPool::GetTotalBits(
    uint64_t lockedTimestamp,
    const std::set<EntropySource> &includedSources) const {
  // Note: We can't reuse the other functions easily because of mutex deadlock
  // if we call them directly
  std::lock_guard<std::mutex> lock(m_mutex);

  size_t count = 0;
  for (const auto &point : m_data) {
    if (point.timestamp <= lockedTimestamp) {
      // Locked data: Always included
      count++;
    } else {
      // New data: Only included if source is enabled
      if (includedSources.find(point.source) != includedSources.end()) {
        count++;
      }
    }
  }

  return static_cast<float>(count) * 2.0f;
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
