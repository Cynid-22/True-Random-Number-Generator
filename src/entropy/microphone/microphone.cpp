#include "microphone.h"
#include "../../logging/logger.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include "../../crypto/secure_mem.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")

// Not provided by MinGW
static const GUID KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID =
{ 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID KSDATAFORMAT_SUBTYPE_PCM_GUID =
{ 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const PROPERTYKEY DEVICE_FORMAT_PROPERTY =
{ { 0xf19f064d, 0x082c, 0x4e27, { 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c } }, 0 };
static const PROPERTYKEY RAW_PROCESSING_SUPPORTED_PROPERTY =
{ { 0x8943b373, 0x388c, 0x4395, { 0xb5, 0x57, 0xbc, 0x6d, 0xba, 0xff, 0xaf, 0xdb } }, 2 };

namespace Entropy {

enum class SampleEncoding { Unsupported, Int16, Int24, Int32, Float32 };

struct SampleFormat {
    SampleEncoding encoding;
    unsigned paddingBits;
    unsigned resolutionBits;
    double fullScale;
};

struct MicrophoneSample {
    double level;
    uint64_t leastSignificantBit;
};

struct DeviceCapabilities {
    bool rawProcessingSupported;
    unsigned resolutionBits;
};

static const unsigned DEFAULT_RESOLUTION_BITS = 16;
static const unsigned MINIMUM_RESOLUTION_BITS = 8;
static const unsigned FLOAT_MANTISSA_BITS = 24;
static const REFERENCE_TIME CAPTURE_BUFFER_DURATION = 10000000;
static const double MINIMUM_RMS_LEVEL = 2.0 / 32768.0;
static const int LSB_BITS_PER_POINT = 64;
static const unsigned WARM_UP_DISCARD_MS = 500;
static const unsigned MILLISECONDS_PER_SECOND = 1000;
static const uint32_t INT24_SIGN_BIT = 0x800000;
static const int32_t INT24_RANGE = 0x1000000;

static double FullScaleOf(unsigned bits) {
    return std::ldexp(1.0, static_cast<int>(bits) - 1);
}

static SampleFormat SampleFormatOf(const WAVEFORMATEX* format, unsigned floatResolutionBits) {
    const size_t extensibleExtraBytes = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    const WAVEFORMATEXTENSIBLE* extensible =
        (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= extensibleExtraBytes)
            ? reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format)
            : nullptr;
    const bool isFloat = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                         (extensible && extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_GUID);
    const bool isInteger = format->wFormatTag == WAVE_FORMAT_PCM ||
                           (extensible && extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM_GUID);
    const unsigned containerBits = format->wBitsPerSample;

    if (isFloat && containerBits == 32)
        return {SampleEncoding::Float32, 0, floatResolutionBits, FullScaleOf(floatResolutionBits)};
    if (!isInteger)
        return {SampleEncoding::Unsupported, 0, 0, 1.0};

    unsigned validBits = containerBits;
    if (extensible && extensible->Samples.wValidBitsPerSample > 0 &&
        extensible->Samples.wValidBitsPerSample <= containerBits)
        validBits = extensible->Samples.wValidBitsPerSample;
    const unsigned paddingBits = containerBits - validBits;
    const double fullScale = FullScaleOf(containerBits);

    switch (containerBits) {
    case 16:
        return {SampleEncoding::Int16, paddingBits, validBits, fullScale};
    case 24:
        return {SampleEncoding::Int24, paddingBits, validBits, fullScale};
    case 32:
        return {SampleEncoding::Int32, paddingBits, validBits, fullScale};
    }
    return {SampleEncoding::Unsupported, 0, 0, 1.0};
}

static DeviceCapabilities ReadDeviceCapabilities(IMMDevice* device) {
    DeviceCapabilities capabilities = {false, DEFAULT_RESOLUTION_BITS};
    IPropertyStore* properties = NULL;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &properties))) {
        return capabilities;
    }

    PROPVARIANT rawSupported;
    PropVariantInit(&rawSupported);
    if (SUCCEEDED(properties->GetValue(RAW_PROCESSING_SUPPORTED_PROPERTY, &rawSupported)) &&
        rawSupported.vt == VT_BOOL) {
        capabilities.rawProcessingSupported = rawSupported.boolVal == VARIANT_TRUE;
    }
    PropVariantClear(&rawSupported);

    PROPVARIANT deviceFormat;
    PropVariantInit(&deviceFormat);
    if (SUCCEEDED(properties->GetValue(DEVICE_FORMAT_PROPERTY, &deviceFormat)) &&
        deviceFormat.vt == VT_BLOB && deviceFormat.blob.cbSize >= sizeof(WAVEFORMATEX)) {
        const WAVEFORMATEX* format = reinterpret_cast<const WAVEFORMATEX*>(deviceFormat.blob.pBlobData);
        if (deviceFormat.blob.cbSize >= sizeof(WAVEFORMATEX) + format->cbSize) {
            const unsigned deviceBits = SampleFormatOf(format, FLOAT_MANTISSA_BITS).resolutionBits;
            if (deviceBits >= MINIMUM_RESOLUTION_BITS) {
                capabilities.resolutionBits = std::min(deviceBits, FLOAT_MANTISSA_BITS);
            }
        }
    }
    PropVariantClear(&deviceFormat);

    properties->Release();
    return capabilities;
}

static HRESULT RequestRawStream(IAudioClient* audioClient) {
    IAudioClient2* audioClient2 = NULL;
    HRESULT hr = audioClient->QueryInterface(__uuidof(IAudioClient2), (void**)&audioClient2);
    if (FAILED(hr)) {
        return hr;
    }

    AudioClientProperties clientProperties = {};
    clientProperties.cbSize = sizeof(clientProperties);
    clientProperties.bIsOffload = FALSE;
    clientProperties.eCategory = AudioCategory_Other;
    clientProperties.Options = AUDCLNT_STREAMOPTIONS_RAW;
    hr = audioClient2->SetClientProperties(&clientProperties);
    audioClient2->Release();
    return hr;
}

static bool OpenCaptureStream(IMMDevice* device, bool raw, unsigned floatResolutionBits,
                              IAudioClient** audioClientOut, WAVEFORMATEX** mixFormatOut,
                              SampleFormat* sampleFormatOut) {
    const Logger::Level failureLevel = raw ? Logger::Level::WARN : Logger::Level::ERR;
    const char* streamName = raw ? "raw" : "standard";
    IAudioClient* audioClient = NULL;
    WAVEFORMATEX* mixFormat = NULL;

    HRESULT hr = device->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL,
        NULL, (void**)&audioClient);
    if (FAILED(hr)) {
        Logger::Log(failureLevel, "Microphone", "Failed to activate AudioClient (%s stream). Error: 0x%08X", streamName, hr);
        return false;
    }

    if (raw) {
        hr = RequestRawStream(audioClient);
        if (FAILED(hr)) {
            Logger::Log(failureLevel, "Microphone", "Failed to request the raw stream. Error: 0x%08X", hr);
            audioClient->Release();
            return false;
        }
    }

    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr)) {
        Logger::Log(failureLevel, "Microphone", "Failed to get MixFormat (%s stream). Error: 0x%08X", streamName, hr);
        audioClient->Release();
        return false;
    }

    const SampleFormat sampleFormat = SampleFormatOf(mixFormat, floatResolutionBits);
    if (sampleFormat.encoding == SampleEncoding::Unsupported) {
        Logger::Log(failureLevel, "Microphone", "Unsupported audio format (%s stream): %u-bit samples",
                    streamName, static_cast<unsigned>(mixFormat->wBitsPerSample));
        CoTaskMemFree(mixFormat);
        audioClient->Release();
        return false;
    }

    hr = audioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        CAPTURE_BUFFER_DURATION,
        0,
        mixFormat,
        NULL);
    if (FAILED(hr)) {
        Logger::Log(failureLevel, "Microphone", "Failed to initialize AudioClient (%s stream). Error: 0x%08X", streamName, hr);
        CoTaskMemFree(mixFormat);
        audioClient->Release();
        return false;
    }

    *audioClientOut = audioClient;
    *mixFormatOut = mixFormat;
    *sampleFormatOut = sampleFormat;
    return true;
}

static MicrophoneSample ReadFirstChannelSample(const BYTE* frame, const SampleFormat& format) {
    int32_t value = 0;
    switch (format.encoding) {
    case SampleEncoding::Int16: {
        int16_t sample;
        memcpy(&sample, frame, sizeof(sample));
        value = sample;
        break;
    }
    case SampleEncoding::Int24: {
        const uint32_t raw = static_cast<uint32_t>(frame[0]) |
                             (static_cast<uint32_t>(frame[1]) << 8) |
                             (static_cast<uint32_t>(frame[2]) << 16);
        value = (raw & INT24_SIGN_BIT) ? static_cast<int32_t>(raw) - INT24_RANGE
                                       : static_cast<int32_t>(raw);
        break;
    }
    case SampleEncoding::Int32:
        memcpy(&value, frame, sizeof(value));
        break;
    case SampleEncoding::Float32: {
        float sample;
        memcpy(&sample, frame, sizeof(sample));
        const double clamped =
            std::isfinite(sample) ? std::max(-1.0, std::min(1.0, static_cast<double>(sample))) : 0.0;
        value = static_cast<int32_t>(std::lrint(clamped * format.fullScale));
        break;
    }
    case SampleEncoding::Unsupported:
        break;
    }
    const uint32_t bits = static_cast<uint32_t>(value);
    return {std::fabs(static_cast<double>(value)) / format.fullScale,
            (bits >> format.paddingBits) & 1u};
}

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

bool MicrophoneCollector::IsRawStream() const {
    return m_rawStream;
}

unsigned MicrophoneCollector::GetResolutionBits() const {
    return m_resolutionBits;
}

void MicrophoneCollector::MarkCaptureFailed() {
    m_lastFailureMs = SteadyClockMilliseconds();
    m_running = false;
}

EntropyDataVector MicrophoneCollector::Harvest() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_buffer.empty()) {
        return {};
    }

    EntropyDataVector harvested;
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
    SampleFormat sampleFormat = {SampleEncoding::Unsupported, 0, 0, 1.0};

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

    const DeviceCapabilities capabilities = ReadDeviceCapabilities(captureDevice);
    const bool rawStream = capabilities.rawProcessingSupported &&
                           OpenCaptureStream(captureDevice, true, capabilities.resolutionBits,
                                             &audioClient, &mixFormat, &sampleFormat);
    if (capabilities.rawProcessingSupported && !rawStream) {
        Logger::Log(Logger::Level::WARN, "Microphone", "Raw stream unavailable, using the standard stream.");
    }
    if (!rawStream &&
        !OpenCaptureStream(captureDevice, false, capabilities.resolutionBits,
                           &audioClient, &mixFormat, &sampleFormat)) {
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

    Logger::Log(Logger::Level::INFO, "Microphone", "Audio capture started (%s stream, LSB of %u-bit samples). Initializing loop...",
                rawStream ? "raw" : "standard", sampleFormat.resolutionBits);
    m_lastFailureMs = 0;
    m_rawStream = rawStream;
    m_resolutionBits = sampleFormat.resolutionBits;
    m_capturing = true;

    UINT32 packetLength = 0;
    BYTE *frameData;
    UINT32 frameCount;
    DWORD bufferFlags;

    uint64_t lsbAccumulator = 0;
    int accumulatedBitCount = 0;
    // Restart test: LSBs stay correlated for ~175 ms after a start while the input settles
    uint64_t warmUpFramesLeft =
        static_cast<uint64_t>(mixFormat->nSamplesPerSec) * WARM_UP_DISCARD_MS / MILLISECONDS_PER_SECOND;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        hr = captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) continue;

        double pollPeakLevel = 0.0;
        while (packetLength != 0) {
            hr = captureClient->GetBuffer(
                &frameData,
                &frameCount,
                &bufferFlags,
                NULL,
                NULL);

            if (FAILED(hr)) break;

            const UINT32 warmUpFrames = static_cast<UINT32>(std::min<uint64_t>(frameCount, warmUpFramesLeft));
            warmUpFramesLeft -= warmUpFrames;

            if (!(bufferFlags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                const size_t bytesPerFrame = mixFormat->nBlockAlign;

                double sumOfSquares = 0.0;
                int sampleCount = 0;

                EntropyDataVector newPoints;
                newPoints.reserve(frameCount / LSB_BITS_PER_POINT + 1);

                for (UINT32 i = 0; i < frameCount; i++) {
                    const MicrophoneSample sample =
                        ReadFirstChannelSample(frameData + i * bytesPerFrame, sampleFormat);

                    pollPeakLevel = std::max(pollPeakLevel, sample.level);
                    if (i < warmUpFrames) {
                        continue;
                    }
                    sumOfSquares += sample.level * sample.level;
                    sampleCount++;

                    lsbAccumulator = (lsbAccumulator << 1) | sample.leastSignificantBit;
                    accumulatedBitCount++;

                    if (accumulatedBitCount >= LSB_BITS_PER_POINT) {
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

                if (rms > MINIMUM_RMS_LEVEL) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_buffer.insert(m_buffer.end(), newPoints.begin(), newPoints.end());
                }

                Crypto::SecureClearVector(newPoints);
            }

            hr = captureClient->ReleaseBuffer(frameCount);
            if (FAILED(hr)) break;

            hr = captureClient->GetNextPacketSize(&packetLength);
        }
        m_inputPeakLevel = (float)std::min(pollPeakLevel, 1.0);
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
