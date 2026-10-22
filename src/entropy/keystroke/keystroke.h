#pragma once
#include <atomic>
#include <bitset>
#include <mutex>
#include <vector>
#include <windows.h>
#include "../entropy_common.h"
#include "../raw_input_thread.h"

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
    static size_t PhysicalKeyIndex(const RAWKEYBOARD& key);
    void ProcessKey(const RAWINPUT& input, uint64_t timestamp);
    void OnKeyDown(uint64_t timestamp);
    void OnKeyUp(uint64_t timestamp);
    void SecureClearBuffer();

    static constexpr size_t SCAN_CODE_COUNT = 256;
    static constexpr size_t SCAN_CODE_PREFIX_COUNT = 3;
    static constexpr size_t PHYSICAL_KEY_COUNT = SCAN_CODE_COUNT * SCAN_CODE_PREFIX_COUNT;

    std::atomic<bool> m_running{false};
    RawInputThread m_inputThread;
    std::mutex m_mutex;
    EntropyDataVector m_buffer;

    std::bitset<PHYSICAL_KEY_COUNT> m_keysDown;
    uint64_t m_lastKeyDownTime = 0;
    uint64_t m_lastKeyUpTime = 0;
};

}
