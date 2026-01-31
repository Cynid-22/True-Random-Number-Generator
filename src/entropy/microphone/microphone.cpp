#include "microphone.h"
#include "../../logging/logger.h"
#include <iostream>
#include <cmath>
#include <vector>

// Link against required libraries
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")

// GUID for CLSID_MMDeviceEnumerator causing issues with some MinGW setups
// Defining it manually if needed, but usually available in uuid.lib or predefined
// const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
// const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
// const IID IID_IAudioClient = __uuidof(IAudioClient);
// const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

// Fix for MinGW missing GUID
static const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID = 
{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

namespace Entropy {

MicrophoneCollector::MicrophoneCollector() {}

MicrophoneCollector::~MicrophoneCollector() {
    Stop();
    SecureClearBuffer();
}

void MicrophoneCollector::Start() {
    if (m_running.exchange(true)) {
        return; // Already running
    }

    Logger::Log(Logger::Level::INFO, "Microphone", "Starting audio capture thread...");
    m_captureThread = std::thread(&MicrophoneCollector::CaptureThread, this);
}

void MicrophoneCollector::Stop() {
    if (!m_running.exchange(false)) {
        return; // Not running
    }

    Logger::Log(Logger::Level::INFO, "Microphone", "Stopping audio capture...");
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
    
    // Log final stats
    uint64_t count = m_sampleCount.load();
    Logger::Log(Logger::Level::INFO, "Microphone", 
        "COLLECTION STOPPED | Samples: %llu | Rate: %.2f/s", 
        count, m_rate.load());
        
    SecureClearBuffer();
}

bool MicrophoneCollector::IsRunning() const {
    return m_running;
}

std::vector<EntropyDataPoint> MicrophoneCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }
    
    std::vector<EntropyDataPoint> harvested;
    harvested.swap(m_buffer);
    return harvested;
}

void MicrophoneCollector::SecureClearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_buffer.empty()) {
        SecureZeroMemory(m_buffer.data(), m_buffer.size() * sizeof(EntropyDataPoint));
        m_buffer.clear();
    }
}

double MicrophoneCollector::GetEntropyRate() const {
    return m_rate;
}

uint64_t MicrophoneCollector::GetSampleCount() const {
    return m_sampleCount;
}

void MicrophoneCollector::CaptureThread() {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pwfx = NULL;

    CoInitialize(NULL);

    // Get enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to create MMDeviceEnumerator. Error: 0x%08X", hr);
        m_running = false;
        CoUninitialize();
        return;
    }

    // Get default capture device
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to get default audio endpoint. No microphone? Error: 0x%08X", hr);
        pEnumerator->Release();
        m_running = false;
        CoUninitialize();
        return;
    }

    // Activate Audio Client
    hr = pDevice->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL,
        NULL, (void**)&pAudioClient);
    
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to activate AudioClient. Error: 0x%08X", hr);
        pDevice->Release();
        pEnumerator->Release();
        m_running = false;
        CoUninitialize();
        return;
    }

    // Get Mix Format
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to get MixFormat. Error: 0x%08X", hr);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        m_running = false;
        CoUninitialize();
        return;
    }

    // Initialize Audio Client (Shared Mode, Capture)
    // REFTIMES_PER_SEC = 10000000NS
    // Requested Duration = 1 second (buffer size)
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        10000000, 
        0,
        pwfx,
        NULL);
    
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to initialize AudioClient. Error: 0x%08X", hr);
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        m_running = false;
        CoUninitialize();
        return;
    }

    // Get Capture Client
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**)&pCaptureClient);
        
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to get AudioCaptureClient. Error: 0x%08X", hr);
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        m_running = false;
        CoUninitialize();
        return;
    }

    // Start Recording
    hr = pAudioClient->Start();
    if (FAILED(hr)) {
        Logger::Log(Logger::Level::ERR, "Microphone", "Failed to start recording. Error: 0x%08X", hr);
        pCaptureClient->Release();
        CoTaskMemFree(pwfx);
        pAudioClient->Release();
        pDevice->Release();
        pEnumerator->Release();
        m_running = false;
        CoUninitialize();
        return;
    }

    Logger::Log(Logger::Level::INFO, "Microphone", "Audio capture started. Initializing loop...");

    UINT32 packetLength = 0;
    BYTE *pData;
    UINT32 numFramesAvailable;
    DWORD flags;

    // Stats tracking
    auto lastRateTime = std::chrono::steady_clock::now();
    uint64_t samplesSinceLastRateCheck = 0;
    
    // Bit packing state
    uint64_t entropyAccumulator = 0;
    int bitsCollected = 0;

    while (m_running) {
        // Sleep for half the buffer duration (approx 10ms for low latency)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) continue;

        while (packetLength != 0) {
            hr = pCaptureClient->GetBuffer(
                &pData,
                &numFramesAvailable,
                &flags,
                NULL,
                NULL);

            if (FAILED(hr)) break;

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Silent buffer, ignore
            } else {
                // Process audio data
                int bytesPerFrame = pwfx->nBlockAlign;
                int bitsPerSample = pwfx->wBitsPerSample;

                // Simple RMS check for "dead" mic
                double sumSquares = 0.0;
                int validSamples = 0;

                std::vector<EntropyDataPoint> newPoints;
                newPoints.reserve(numFramesAvailable / 64 + 1); // Approx packing ratio

                for (UINT32 i = 0; i < numFramesAvailable; i++) {
                    // Extract first channel sample
                    int32_t sampleVal = 0;
                    
                    if (bitsPerSample == 16) {
                        int16_t* ptr = (int16_t*)(pData + i * bytesPerFrame);
                        sampleVal = *ptr;
                    } else if (bitsPerSample == 32) {
                        // Float or Int32? Usually float for WASAPI shared
                        // Use our local GUID definition
                        if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT || 
                            (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && 
                             ((WAVEFORMATEXTENSIBLE*)pwfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID)) {
                            float* ptr = (float*)(pData + i * bytesPerFrame);
                            sampleVal = (int32_t)(*ptr * 32767.0f);
                        } else {
                            int32_t* ptr = (int32_t*)(pData + i * bytesPerFrame);
                            sampleVal = *ptr;
                        }
                    }

                    sumSquares += (double)sampleVal * sampleVal;
                    validSamples++;

                    // OPTIMIZATION: Pack bits instead of 1 event per sample
                    // Extract LSB (0 or 1)
                    uint64_t lsb = (std::abs(sampleVal) & 1);
                    
                    // Shift into accumulator
                    entropyAccumulator = (entropyAccumulator << 1) | lsb;
                    bitsCollected++;
                    
                    // When we have 64 bits, push a data point
                    if (bitsCollected >= 64) {
                        EntropyDataPoint pt;
                        pt.timestamp = GetNanosecondTimestamp();
                        pt.value = entropyAccumulator; // 64 bits of entropy
                        pt.source = EntropySource::Microphone;
                        newPoints.push_back(pt);
                        
                        entropyAccumulator = 0;
                        bitsCollected = 0;
                    }
                }

                // RMS Check
                double rms = 0.0;
                if (validSamples > 0) {
                    rms = std::sqrt(sumSquares / validSamples);
                }

                // Threshold for "dead" mic (digital silence or near silence)
                if (rms > 2.0) { 
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_buffer.insert(m_buffer.end(), newPoints.begin(), newPoints.end());
                    m_sampleCount += newPoints.size(); // Count EVENTS, not raw samples now
                    samplesSinceLastRateCheck += newPoints.size();
                }
            }

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr)) break;

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
        }

        // Update rate stats once per second
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastRateTime).count();
        if (elapsed >= 1) {
            m_rate = (double)samplesSinceLastRateCheck / (double)elapsed;
            samplesSinceLastRateCheck = 0;
            lastRateTime = now;
        }
    }

    // Cleanup
    if (pAudioClient) pAudioClient->Stop();
    
    CoTaskMemFree(pwfx);
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    
    CoUninitialize();
    Logger::Log(Logger::Level::INFO, "Microphone", "Capture thread exited.");
}

} // namespace Entropy
