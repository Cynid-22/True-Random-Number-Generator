#pragma once
#include <atomic>
#include <bitset>
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

    EntropyDataVector Harvest();

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    void ProcessKeyMessage(WPARAM message, const KBDLLHOOKSTRUCT& keyEvent);
    void OnKeyDown(uint64_t timestamp);
    void OnKeyUp(uint64_t timestamp);
    void SecureClearBuffer();

    static constexpr size_t VIRTUAL_KEY_COUNT = 256;

    std::atomic<bool> m_running{false};
    HHOOK m_hook = nullptr;
    std::mutex m_mutex;
    EntropyDataVector m_buffer;

    std::bitset<VIRTUAL_KEY_COUNT> m_keysDown;
    uint64_t m_lastKeyDownTime = 0;
    uint64_t m_lastKeyUpTime = 0;

    static KeystrokeCollector* s_instance;
};

}
