#include "pool.h"
#include "../../logging/logger.h"
#include <windows.h> // For SecureZeroMemory
#include <algorithm>
#include <cmath>

namespace Entropy {

EntropyPool::EntropyPool() {
}

EntropyPool::~EntropyPool() {
    SecureWipe();
}

void EntropyPool::AddDataPoint(const EntropyDataPoint& point) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.push_back(point);
    // Sort after adding to maintain chronological order
    EnsureSorted();
}

void EntropyPool::AddDataPoints(const std::vector<EntropyDataPoint>& points) {
    if (points.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.insert(m_data.end(), points.begin(), points.end());
    // Sort after bulk add to maintain chronological order
    EnsureSorted();
}

std::vector<EntropyDataPoint> EntropyPool::GetPooledData() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data; // Return copy (data is already sorted)
}

std::vector<EntropyDataPoint> EntropyPool::GetPooledDataForSources(const std::set<EntropySource>& includedSources) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<EntropyDataPoint> filtered;
    filtered.reserve(m_data.size()); // Reserve space for efficiency
    
    for (const auto& point : m_data) {
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
    
    if (m_data.empty()) return 0.0f;
    
    // Conservative estimate: ~2 bits per data point
    // This matches the estimate used in CalculateEntropyFromDeltas
    return static_cast<float>(m_data.size()) * 2.0f;
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
        [](const EntropyDataPoint& a, const EntropyDataPoint& b) {
            return a.timestamp < b.timestamp;
        });
}

} // namespace Entropy
