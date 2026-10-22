#pragma once
#include <cstdint>
#include <functional>
#include <future>
#include <thread>
#include <vector>
#include <windows.h>

namespace Entropy {

struct HidUsage {
    USHORT page;
    USHORT usage;
};

class RawInputThread {
public:
    using InputHandler = std::function<void(const RAWINPUT& input, uint64_t timestamp)>;

    RawInputThread() = default;
    ~RawInputThread();
    RawInputThread(const RawInputThread&) = delete;
    RawInputThread& operator=(const RawInputThread&) = delete;

    bool Start(std::vector<HidUsage> devices, InputHandler handler, const char* logModule);

    void Stop();

private:
    void Run(std::vector<HidUsage> devices, std::promise<bool> started);
    void DeliverInput(HRAWINPUT rawInput);

    InputHandler m_handler;
    const char* m_logModule = "RawInput";
    std::thread m_thread;
    DWORD m_threadId = 0;
    std::vector<BYTE> m_inputBuffer;
};

}
