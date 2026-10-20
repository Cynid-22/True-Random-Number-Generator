#include "microphone.h"
#include "../../logging/logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>
#include "../../crypto/secure_mem.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")

// Not provided by MinGW
static const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID =
{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

namespace Entropy {

MicrophoneCollector::MicrophoneCollector() {}

MicrophoneCollector::~MicrophoneCollector() {
    Stop();
    SecureClearBuffer();
}

static long long SteadyClockMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static const long long RETRY_AFTER_FAILURE_MS = 3000;

void MicrophoneCollector::Start() {
    if (m_running.load()) {
        return;
    }

    const long long lastFailureMs = m_lastFailureMs.load();
    if (lastFailureMs != 0 && SteadyClockMilliseconds() - lastFailureMs < RETRY_AFTER_FAILURE_MS) {
        return;
    }

    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }

    m_running = true;
    Logger::Log(Logger::Level::INFO, "Microphone", "Starting audio capture thread...");
    m_captureThread = std::thread(&MicrophoneCollector::CaptureLoop, this);
}

void MicrophoneCollector::Stop() {
    const bool wasRunning = m_running.exchange(false);

    if (wasRunning) {
        Logger::Log(Logger::Level::INFO, "Microphone", "Stopping audio capture...");
    }
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
    if (wasRunning) {
        Logger::Log(Logger::Level::INFO, "Microphone", "Collection stopped.");
    }

    SecureClearBuffer();
}

bool MicrophoneCollector::IsRunning() const {
    return m_running;
}

bool MicrophoneCollector::IsCapturing() const {
    return m_capturing;
}

bool MicrophoneCollector::HasFailed() const {
    return m_lastFailureMs.load() != 0;
}

float MicrophoneCollector::GetInputPeakLevel() const {
    return m_inputPeakLevel;
}

void MicrophoneCollector::MarkCaptureFailed() {
    m_lastFailureMs = SteadyClockMilliseconds();
    m_running = false;
}

std::vector<EntropyDataPoint> MicrophoneCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    std::vector<EntropyDataPoint> harvested;
    harvested.swap(m_buffer);
    m_buffer.shrink_to_fit();
    return harvested;
}

void MicrophoneCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    Crypto::SecureClearVector(m_buffer);
}

void MicrophoneCollector::CaptureLoop() {
    HRESULT hr;
    IMMDeviceEnumerator *deviceEnumerator = NULL;
    IMMDevice *captureDevice = NULL;
    IAudioClient *audioClient = NULL;
    IAudioCaptureClient *captureClient = NULL;
    WAVEFORMATEX *mixFormat = NULL;

    CoInitialize(NULL);

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator);

    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to create MMDeviceEnumerator. Error: 0x%08X", hr);
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &captureDevice);
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to get default audio endpoint. No microphone? Error: 0x%08X", hr);
        deviceEnumerator->Release();
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    hr = captureDevice->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL,
        NULL, (void**)&audioClient);

    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to activate AudioClient. Error: 0x%08X", hr);
        captureDevice->Release();
        deviceEnumerator->Release();
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to get MixFormat. Error: 0x%08X", hr);
        audioClient->Release();
        captureDevice->Release();
        deviceEnumerator->Release();
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        10000000,
        0,
        mixFormat,
        NULL);

    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to initialize AudioClient. Error: 0x%08X", hr);
        CoTaskMemFree(mixFormat);
        audioClient->Release();
        captureDevice->Release();
        deviceEnumerator->Release();
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    hr = audioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&captureClient);

    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to get AudioCaptureClient. Error: 0x%08X", hr);
        CoTaskMemFree(mixFormat);
        audioClient->Release();
        captureDevice->Release();
        deviceEnumerator->Release();
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    hr = audioClient->Start();
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to start recording. Error: 0x%08X", hr);
        captureClient->Release();
        CoTaskMemFree(mixFormat);
        audioClient->Release();
        captureDevice->Release();
        deviceEnumerator->Release();
        MarkCaptureFailed();
        CoUninitialize();
        return;
    }

    Logger::Log(Logger::Level::INFO, "Microphone", "Audio capture started. Initializing loop...");
    m_lastFailureMs = 0;
    m_capturing = true;

    const bool isFloatFormat =
        mixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
        (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
         ((WAVEFORMATEXTENSIBLE*)mixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID);
    const double fullScaleAmplitude =
        (mixFormat->wBitsPerSample == 32 && !isFloatFormat) ? (double)INT32_MAX : (double)INT16_MAX;

    UINT32 packetLength = 0;
    BYTE *frameData;
    UINT32 frameCount;
    DWORD bufferFlags;

    uint64_t lsbAccumulator = 0;
    int accumulatedBitCount = 0;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        hr = captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) continue;

        double pollPeakAmplitude = 0.0;
        while (packetLength != 0) {
            hr = captureClient->GetBuffer(
                &frameData,
                &frameCount,
                &bufferFlags,
                NULL,
                NULL);

            if (FAILED(hr)) break;

            if (!(bufferFlags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                int bytesPerFrame = mixFormat->nBlockAlign;
                int bitsPerSample = mixFormat->wBitsPerSample;

                double sumOfSquares = 0.0;
                int sampleCount = 0;

                std::vector<EntropyDataPoint> newPoints;
                newPoints.reserve(frameCount / 64 + 1);

                for (UINT32 i = 0; i < frameCount; i++) {
                    int32_t sampleValue = 0;

                    if (bitsPerSample == 16) {
                        int16_t* sample = (int16_t*)(frameData + i * bytesPerFrame);
                        sampleValue = *sample;
                    } else if (bitsPerSample == 32) {
                        if (isFloatFormat) {
                            float* sample = (float*)(frameData + i * bytesPerFrame);
                            sampleValue = (int32_t)(*sample * 32767.0f);
                        } else {
                            int32_t* sample = (int32_t*)(frameData + i * bytesPerFrame);
                            sampleValue = *sample;
                        }
                    }

                    pollPeakAmplitude = std::max(pollPeakAmplitude, std::fabs((double)sampleValue));
                    sumOfSquares += (double)sampleValue * sampleValue;
                    sampleCount++;

                    uint64_t leastSignificantBit = (std::abs(sampleValue) & 1);

                    lsbAccumulator = (lsbAccumulator << 1) | leastSignificantBit;
                    accumulatedBitCount++;

                    if (accumulatedBitCount >= 64) {
                        EntropyDataPoint dataPoint;
                        dataPoint.timestamp = GetNanosecondTimestamp();
                        dataPoint.value = lsbAccumulator;
                        dataPoint.source = EntropySource::Microphone;
                        newPoints.push_back(dataPoint);

                        lsbAccumulator = 0;
                        accumulatedBitCount = 0;
                    }
                }

                double rms = 0.0;
                if (sampleCount > 0) {
                    rms = std::sqrt(sumOfSquares / sampleCount);
                }

                if (rms > 2.0) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_buffer.insert(m_buffer.end(), newPoints.begin(), newPoints.end());
                }

                Crypto::SecureClearVector(newPoints);
            }

            hr = captureClient->ReleaseBuffer(frameCount);
            if (FAILED(hr)) break;

            hr = captureClient->GetNextPacketSize(&packetLength);
        }
        m_inputPeakLevel = (float)std::min(pollPeakAmplitude / fullScaleAmplitude, 1.0);
    }

    m_capturing = false;
    m_inputPeakLevel = 0.0f;
    if (audioClient) audioClient->Stop();

    CoTaskMemFree(mixFormat);
    if (captureClient) captureClient->Release();
    if (audioClient) audioClient->Release();
    if (captureDevice) captureDevice->Release();
    if (deviceEnumerator) deviceEnumerator->Release();

    CoUninitialize();
    Logger::Log(Logger::Level::INFO, "Microphone", "Capture thread exited.");
}

}
