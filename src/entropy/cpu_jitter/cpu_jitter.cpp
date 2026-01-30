#include "cpu_jitter.h"
#include "../../logging/logger.h"
#include <chrono>
#include <windows.h> // For SecureZeroMemory

namespace Entropy {

CpuJitterCollector::CpuJitterCollector() {}

CpuJitterCollector::~CpuJitterCollector() {
    Stop();
    SecureClearBuffer();
}

void CpuJitterCollector::Start() {
    if (m_running) return;
    
    Logger::Log(Logger::Level::INFO, "CpuJitter", "Starting collector threads...");
    
    m_running = true;
    m_paused = false;
    m_counter = 0;
    
    m_runnerThread = std::thread(&CpuJitterCollector::RunnerLoop, this);
    m_refereeThread = std::thread(&CpuJitterCollector::RefereeLoop, this);
}

void CpuJitterCollector::Stop() {
    if (!m_running) return;

    Logger::Log(Logger::Level::INFO, "CpuJitter", "Stopping collector threads...");
    m_running = false;
    
    // Wait for threads to finish
    if (m_runnerThread.joinable()) m_runnerThread.join();
    if (m_refereeThread.joinable()) m_refereeThread.join();

    uint64_t count = m_sampleCount.load();
    Logger::Log(Logger::Level::INFO, "CpuJitter", 
        "COLLECTION STOPPED | Samples: %llu | Rate: %.2f/s", 
        count, m_rate.load());
}

bool CpuJitterCollector::IsRunning() const {
    return m_running;
}

void CpuJitterCollector::RunnerLoop() {
    while (m_running) {
        if (!m_paused) {
            // Tight loop increment - execution speed depends on CPU state/contention
            m_counter.fetch_add(1, std::memory_order_relaxed);
        } else {
            // Yield if paused to behave as "frozen" for the referee
            std::this_thread::yield();
        }
    }
}

void CpuJitterCollector::RefereeLoop() {
    Logger::Log(Logger::Level::INFO, "CpuJitter", "Referee thread started (Race Condition Active)");
    uint64_t lastCount = 0;
    auto lastRateCheck = std::chrono::steady_clock::now();
    uint64_t samplesSinceRateCheck = 0;

    while (m_running) {
        // 1. Sleep ~1ms (OS scheduling jitter source)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        if (!m_running) break;

        // 2. Freeze Runner
        m_paused = true;
        
        // 3. Read Count
        uint64_t currentCount = m_counter.load(std::memory_order_relaxed);
        
        // 4. Unfreeze Runner
        m_paused = false;

        // 5. Calculate Delta (Entropy comes from variation in this delta)
        uint64_t delta = currentCount - lastCount;
        lastCount = currentCount;

        // 6. Capture Data
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_buffer.push_back({
                GetNanosecondTimestamp(),
                delta,
                EntropySource::CpuJitter
            });
        }
        
        // Log occasional sample (every 10th to avoid spamming >1000 logs/sec)
        if (m_sampleCount % 10 == 0) {
             Logger::Log(Logger::Level::DEBUG, "CpuJitter", 
            "Sample #%llu | Delta: %llu", m_sampleCount.load(), delta);
        }
        
        m_sampleCount++;
        samplesSinceRateCheck++;

        // Update Rate every 1 second
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> diff = now - lastRateCheck;
        if (diff.count() >= 1.0) {
            m_rate = samplesSinceRateCheck / diff.count();
            samplesSinceRateCheck = 0;
            lastRateCheck = now;
        }
    }
}

std::vector<EntropyDataPoint> CpuJitterCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<EntropyDataPoint> result = std::move(m_buffer);
    m_buffer.clear(); 
    return result;
}

double CpuJitterCollector::GetEntropyRate() const {
    return m_rate;
}

uint64_t CpuJitterCollector::GetSampleCount() const {
    return m_sampleCount;
}

void CpuJitterCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_buffer.empty()) {
        SecureZeroMemory(m_buffer.data(), m_buffer.size() * sizeof(EntropyDataPoint));
        m_buffer.clear();
    }
}

} // namespace Entropy
