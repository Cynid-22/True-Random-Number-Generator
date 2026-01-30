#include <bitset> // Added for binary formatting
#include "clock_drift.h"
#include "../../logging/logger.h"
#include <intrin.h> // For __rdtsc
#include <chrono>
#include <windows.h> // For SecureZeroMemory

namespace Entropy {

ClockDriftCollector::ClockDriftCollector() {
}

ClockDriftCollector::~ClockDriftCollector() {
    Stop();
    SecureClearBuffer();
}

void ClockDriftCollector::Start() {
    if (m_running.exchange(true)) {
        return; // Already running
    }

    Logger::Log(Logger::Level::INFO, "ClockDrift", "Starting collector thread...");
    m_thread = std::thread(&ClockDriftCollector::CollectionLoop, this);
}

void ClockDriftCollector::Stop() {
    if (!m_running.exchange(false)) {
        return; // Not running
    }

    Logger::Log(Logger::Level::INFO, "ClockDrift", "Stopping collector thread...");
    if (m_thread.joinable()) {
        m_thread.join();
    }
    
    // Log final summary as requested
    uint64_t count = m_sampleCount.load();
    // Raw bits = 16 bits per sample (what we capture)
    // Entropy estimate = ~2 bits per sample (what we assume is truly random)
    Logger::Log(Logger::Level::INFO, "ClockDrift", 
        "COLLECTION STOPPED | Samples: %llu | Rate: %.2f/s | Raw Data: %llu bits | Entropy Est: %.0f bits", 
        count, m_rate.load(), count * 16, count * 2.0f);
        
    Logger::Log(Logger::Level::INFO, "ClockDrift", "Collector thread stopped.");
    
    // Securely clear any remaining buffer data
    SecureClearBuffer();
}

bool ClockDriftCollector::IsRunning() const {
    return m_running;
}

std::vector<EntropyDataPoint> ClockDriftCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }
    
    std::vector<EntropyDataPoint> harvested;
    harvested.swap(m_buffer); // Efficient move
    return harvested;
}

void ClockDriftCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_buffer.empty()) {
        // Securely zero all memory before clearing
        SecureZeroMemory(m_buffer.data(), m_buffer.size() * sizeof(EntropyDataPoint));
        m_buffer.clear();
        m_buffer.shrink_to_fit(); // Release memory
    }
}

double ClockDriftCollector::GetEntropyRate() const {
    return m_rate;
}

uint64_t ClockDriftCollector::GetSampleCount() const {
    return m_sampleCount;
}

void ClockDriftCollector::CollectionLoop() {
    Logger::Log(Logger::Level::INFO, "ClockDrift", "Thread main loop started");
    
    auto lastRateCheck = std::chrono::steady_clock::now();
    uint64_t lastSampleCount = 0;
    
    while (m_running) {
        // 1. Read CPU cycle counter BEFORE sleep
        uint64_t before = __rdtsc();
        
        // 2. Sleep for ~1ms using OS timer
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        // 3. Read CPU cycle counter AFTER sleep
        uint64_t after = __rdtsc();
        
        // 4. Calculate delta (this captures the jitter)
        uint64_t delta = after - before;
        
        // 5. Basic sanity check
        if (delta == 0 || delta > 1000000000) { 
             Logger::Log(Logger::Level::WARN, "ClockDrift", "Anomalous delta detected: %llu", delta);
             continue;
        }

        // 6. Extract entropy from least significant bits
        // We take the lower 16 bits as the raw entropy data point.
        uint64_t entropyPoint = delta & 0xFFFF;
        
        // 7. Capture timestamp
        uint64_t timestamp = GetNanosecondTimestamp();
        
        // Log details for every 10th sample (more detailed as requested)
        if (m_sampleCount % 10 == 0) {
             // Create 16-bit binary representation
        std::bitset<16> binary(entropyPoint);
             
             Logger::Log(Logger::Level::DEBUG, "ClockDrift", 
            "Sample #%llu | Delta: %llu | Entropy: 0x%04X | Binary: %s | Timestamp: %llu", 
            m_sampleCount.load(), delta, entropyPoint, binary.to_string().c_str(), timestamp);
        }

        // 8. Store sample with timestamp and source
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            EntropyDataPoint dataPoint;
            dataPoint.timestamp = timestamp;
            dataPoint.value = entropyPoint;
            dataPoint.source = EntropySource::ClockDrift;
            m_buffer.push_back(dataPoint);
        }
        m_sampleCount++;

        // Rate calculation (approximate)
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRateCheck).count();
        if (elapsed >= 1000) {
            uint64_t count = m_sampleCount;
            uint64_t diff = count - lastSampleCount;
            m_rate = (double)diff * 1000.0 / elapsed;
            
            lastSampleCount = count;
            lastRateCheck = now;
            
            lastSampleCount = count;
            lastRateCheck = now;
            
            // Stats summary removed from loop per user request
            // Logger::Log(Logger::Level::DEBUG, "ClockDrift", 
            //    "STATS | Rate: %.2f samples/sec | Total Samples: %llu | Total Bits Est: %llu", 
            //    m_rate.load(), count, count * 16);
        }
    }
}

} // namespace Entropy
