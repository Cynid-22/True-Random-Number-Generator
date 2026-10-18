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

    std::vector<EntropyDataPoint> Harvest();

private:
    void CaptureLoop();
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    std::thread m_captureThread;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;
};

}
