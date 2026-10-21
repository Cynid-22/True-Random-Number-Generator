#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <windows.h>
#include "entropy_common.h"

namespace Entropy {

class SampleRecorder {
public:
    static constexpr size_t RESTART_COUNT = 1000;
    static constexpr size_t RESTART_ROW_SAMPLES = 1000;
    static constexpr size_t MICROPHONE_RESTART_ROW_VALUES = 16;

    enum class RowStatus { Filling, Complete, Failed };

    SampleRecorder();
    ~SampleRecorder();
    SampleRecorder(const SampleRecorder&) = delete;
    SampleRecorder& operator=(const SampleRecorder&) = delete;

    bool BeginRecording(const std::wstring& folder,
                        const std::array<bool, ENTROPY_SOURCE_COUNT>& sources);
    bool BeginRestartTest(const std::wstring& folder, EntropySource source);
    void Finish();

    bool Record(const EntropyDataVector& points);
    RowStatus AddToRestartRow(const EntropyDataVector& points);

    bool IsRecording() const { return m_mode == Mode::Recording; }
    bool IsRestartTest() const { return m_mode == Mode::RestartTest; }
    EntropySource RestartSource() const { return m_restartSource; }
    bool RestartRowStarted() const { return m_restartRowFill > 0; }
    size_t RestartsCompleted() const { return m_restartsCompleted; }
    bool RestartTestComplete() const { return m_restartsCompleted >= RESTART_COUNT; }
    uint64_t SamplesWritten(EntropySource source) const;
    const std::string& LastError() const { return m_lastError; }

    static const char* FileStem(EntropySource source);

private:
    enum class Mode { Idle, Recording, RestartTest };

    bool OpenFile(EntropySource source, const std::wstring& path, bool append);
    bool Write(EntropySource source, const EntropyDataPoint* points, size_t count);
    void CloseFiles();

    Mode m_mode = Mode::Idle;
    std::array<HANDLE, ENTROPY_SOURCE_COUNT> m_files;
    std::array<uint64_t, ENTROPY_SOURCE_COUNT> m_samplesWritten{};
    EntropySource m_restartSource = EntropySource::ClockDrift;
    size_t m_restartRowFill = 0;
    size_t m_restartsCompleted = 0;
    std::string m_lastError;
};

}
