#pragma once
#include <chrono>
#include <ratio>
#include <thread>
#include <windows.h>

namespace Entropy {

class HighResolutionSleeper {
public:
    HighResolutionSleeper()
        : m_timer(CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                         TIMER_MODIFY_STATE | SYNCHRONIZE)) {}

    ~HighResolutionSleeper() {
        if (m_timer) {
            CloseHandle(m_timer);
        }
    }

    HighResolutionSleeper(const HighResolutionSleeper&) = delete;
    HighResolutionSleeper& operator=(const HighResolutionSleeper&) = delete;

    bool IsHighResolution() const {
        return m_timer != nullptr;
    }

    void SleepFor(std::chrono::microseconds duration) {
        if (m_timer) {
            using TimerTicks = std::chrono::duration<LONGLONG, std::ratio<1, 10000000>>;
            LARGE_INTEGER relativeDueTime;
            relativeDueTime.QuadPart = -std::chrono::duration_cast<TimerTicks>(duration).count();
            if (SetWaitableTimer(m_timer, &relativeDueTime, 0, nullptr, nullptr, FALSE) &&
                WaitForSingleObject(m_timer, INFINITE) == WAIT_OBJECT_0) {
                return;
            }
        }
        std::this_thread::sleep_for(duration);
    }

private:
    HANDLE m_timer;
};

}
