#include "sample_recorder.h"
#include "../platform/unicode_path.h"
#include <algorithm>

namespace Entropy {

namespace {

constexpr size_t WRITE_CHUNK_VALUES = 512;

size_t SourceIndex(EntropySource source) {
    return static_cast<size_t>(source);
}

size_t RestartRowValues(EntropySource source) {
    return source == EntropySource::Microphone
               ? SampleRecorder::MICROPHONE_RESTART_ROW_VALUES
               : SampleRecorder::RESTART_ROW_SAMPLES;
}

std::wstring SampleFilePath(const std::wstring& folder, EntropySource source,
                            const char* suffix) {
    return folder + L"\\" + Platform::Utf8ToWide(SampleRecorder::FileStem(source)) +
           Platform::Utf8ToWide(suffix);
}

}

SampleRecorder::SampleRecorder() {
    m_files.fill(INVALID_HANDLE_VALUE);
}

SampleRecorder::~SampleRecorder() {
    Finish();
}

const char* SampleRecorder::FileStem(EntropySource source) {
    switch (source) {
    case EntropySource::Microphone: return "mic";
    case EntropySource::Keystroke: return "keystroke";
    case EntropySource::ClockDrift: return "clockdrift";
    case EntropySource::CpuJitter: return "cpujitter";
    case EntropySource::Mouse: return "mouse";
    }
    return "unknown";
}

bool SampleRecorder::BeginRecording(const std::wstring& folder,
                                    const std::array<bool, ENTROPY_SOURCE_COUNT>& sources) {
    Finish();
    m_lastError.clear();
    m_samplesWritten.fill(0);

    for (size_t sourceIndex = 0; sourceIndex < ENTROPY_SOURCE_COUNT; sourceIndex++) {
        if (!sources[sourceIndex])
            continue;
        const EntropySource source = static_cast<EntropySource>(sourceIndex);
        if (!OpenFile(source, SampleFilePath(folder, source, ".u64"), true)) {
            CloseFiles();
            return false;
        }
    }
    m_mode = Mode::Recording;
    return true;
}

bool SampleRecorder::BeginRestartTest(const std::wstring& folder, EntropySource source) {
    Finish();
    m_lastError.clear();
    m_samplesWritten.fill(0);

    if (!OpenFile(source, SampleFilePath(folder, source, "_restart.u64"), false))
        return false;
    m_restartSource = source;
    m_restartRowFill = 0;
    m_restartsCompleted = 0;
    m_mode = Mode::RestartTest;
    return true;
}

void SampleRecorder::Finish() {
    CloseFiles();
    m_mode = Mode::Idle;
}

bool SampleRecorder::Record(const EntropyDataVector& points) {
    if (m_mode != Mode::Recording)
        return false;

    size_t runStart = 0;
    while (runStart < points.size()) {
        size_t runEnd = runStart + 1;
        while (runEnd < points.size() && points[runEnd].source == points[runStart].source)
            runEnd++;
        if (!Write(points[runStart].source, points.data() + runStart, runEnd - runStart)) {
            Finish();
            return false;
        }
        runStart = runEnd;
    }
    return true;
}

SampleRecorder::RowStatus SampleRecorder::AddToRestartRow(const EntropyDataVector& points) {
    if (m_mode != Mode::RestartTest)
        return RowStatus::Failed;

    const size_t rowValues = RestartRowValues(m_restartSource);
    const size_t takenCount = std::min(points.size(), rowValues - m_restartRowFill);
    if (!Write(m_restartSource, points.data(), takenCount)) {
        Finish();
        return RowStatus::Failed;
    }

    m_restartRowFill += takenCount;
    if (m_restartRowFill < rowValues)
        return RowStatus::Filling;

    m_restartRowFill = 0;
    m_restartsCompleted++;
    if (RestartTestComplete())
        Finish();
    return RowStatus::Complete;
}

uint64_t SampleRecorder::SamplesWritten(EntropySource source) const {
    return m_samplesWritten[SourceIndex(source)];
}

bool SampleRecorder::OpenFile(EntropySource source, const std::wstring& path, bool append) {
    const HANDLE file = CreateFileW(path.c_str(), append ? FILE_APPEND_DATA : GENERIC_WRITE,
                                    FILE_SHARE_READ, nullptr,
                                    append ? OPEN_ALWAYS : CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        m_lastError = "Could not open " + Platform::WideToUtf8(path.c_str()) + ":\n" +
                      Platform::SystemErrorText(GetLastError());
        return false;
    }
    m_files[SourceIndex(source)] = file;
    return true;
}

bool SampleRecorder::Write(EntropySource source, const EntropyDataPoint* points, size_t count) {
    const HANDLE file = m_files[SourceIndex(source)];
    if (file == INVALID_HANDLE_VALUE)
        return true;

    std::array<uint64_t, WRITE_CHUNK_VALUES> chunk;
    DWORD errorCode = ERROR_SUCCESS;
    for (size_t offset = 0; offset < count; offset += WRITE_CHUNK_VALUES) {
        const size_t chunkCount = std::min(WRITE_CHUNK_VALUES, count - offset);
        for (size_t i = 0; i < chunkCount; i++)
            chunk[i] = points[offset + i].value;

        const DWORD byteCount = static_cast<DWORD>(chunkCount * sizeof(uint64_t));
        DWORD bytesWritten = 0;
        if (!WriteFile(file, chunk.data(), byteCount, &bytesWritten, nullptr) ||
            bytesWritten != byteCount) {
            errorCode = GetLastError();
            if (errorCode == ERROR_SUCCESS)
                errorCode = ERROR_WRITE_FAULT;
            break;
        }
        m_samplesWritten[SourceIndex(source)] += chunkCount;
    }
    SecureZeroMemory(chunk.data(), sizeof(chunk));

    if (errorCode == ERROR_SUCCESS)
        return true;
    m_lastError = std::string("Could not write ") + FileStem(source) + " samples:\n" +
                  Platform::SystemErrorText(errorCode);
    return false;
}

void SampleRecorder::CloseFiles() {
    for (HANDLE& file : m_files) {
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
    }
}

}
