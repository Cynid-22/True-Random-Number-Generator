#pragma once
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include "../entropy_common.h"

namespace Entropy {

class MicrophoneCollector {
public:
    MicrophoneCollector();
    ~MicrophoneCollector();

    void Start();

    void Stop();

    bool IsRunning() const;

    bool IsCapturing() const;

    bool HasFailed() const;

    float GetInputPeakLevel() const;

    std::vector<EntropyDataPoint> Harvest();

private:
    void CaptureLoop();
    void MarkCaptureFailed();
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_capturing{false};
    std::atomic<long long> m_lastFailureMs{0};
    std::atomic<float> m_inputPeakLevel{0.0f};
    std::thread m_captureThread;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
};

}
