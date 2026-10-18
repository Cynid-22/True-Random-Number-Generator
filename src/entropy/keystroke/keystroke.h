#pragma once
#include <atomic>
#include <mutex>
#include <vector>
#include <windows.h>
#include "../entropy_common.h"

namespace Entropy {

class KeystrokeCollector {
public:
    KeystrokeCollector();
    ~KeystrokeCollector();

    void Start();

    void Stop();

    bool IsRunning() const;

    std::vector<EntropyDataPoint> Harvest();

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ProcessKeyMessage(WPARAM message);
    void OnKeyDown(uint64_t timestamp);
    void OnKeyUp(uint64_t timestamp);
    void SecureClearBuffer();

    std::atomic<bool> m_running{false};
    HHOOK m_hook = nullptr;
    std::mutex m_mutex;
    std::vector<EntropyDataPoint> m_buffer;

    uint64_t m_lastKeyDownTime = 0;
    uint64_t m_lastKeyUpTime = 0;

    static KeystrokeCollector* s_instance;
};

}
