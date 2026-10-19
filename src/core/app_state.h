#pragma once

#include <array>
#include <atomic>
#include <set>
#include <string>
#include <vector>
#include "../entropy/clock_drift/clock_drift.h"
#include "../entropy/cpu_jitter/cpu_jitter.h"
#include "../entropy/keystroke/keystroke.h"
#include "../entropy/mouse/mouse.h"
#include "../entropy/microphone/microphone.h"
#include "../entropy/pool.h"
#include "../crypto/secure_mem.h"
#include "../../config/AppConfig.h"
#include "output_format.h"

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

struct OutputSetting {
    std::string name;
    std::string value;
    bool isText;
};

struct GeneratedOutputDetails {
    int format = FORMAT_DECIMAL;
    std::string category;
    std::string type;
    std::vector<OutputSetting> settings;
    std::string securityMode;
    bool numberedItems = false;
};

struct AppState {
    AppState() {
        passphraseSeparator.assign(AppConfig::PASSPHRASE_SEPARATOR_MAX_BYTES, '\0');
        passphraseSeparator[0] = '-';
        oneTimePadMessage.assign(AppConfig::ONE_TIME_PAD_MESSAGE_MAX_BYTES, '\0');
        oneTimePadFilePath.assign(AppConfig::ONE_TIME_PAD_FILE_PATH_MAX_BYTES, '\0');
        keystrokePreview.reserve(AppConfig::KEYSTROKE_PREVIEW_MAX_CHARACTERS);
    }

    Entropy::ClockDriftCollector clockDriftCollector;
    Entropy::CpuJitterCollector cpuJitterCollector;
    Entropy::KeystrokeCollector keystrokeCollector;
    Entropy::MouseCollector mouseCollector;
    Entropy::MicrophoneCollector microphoneCollector;

    Entropy::EntropyPool entropyPool;

    bool microphoneEnabled = true;
    bool keystrokeEnabled = true;
    bool clockDriftEnabled = true;
    bool cpuJitterEnabled = true;
    bool mouseMovementEnabled = true;

    bool isCollecting = false;
    float collectedEntropyBits = 0.0f;

    float microphoneEntropyBits = 0.0f;
    float keystrokeEntropyBits = 0.0f;
    float clockDriftEntropyBits = 0.0f;
    float cpuJitterEntropyBits = 0.0f;
    float mouseEntropyBits = 0.0f;

    struct TrailPoint { float x, y; };
    std::vector<TrailPoint> mouseTrail;
    std::vector<char> keystrokePreview;

    float targetEntropyBits = 512.0f;

    int outputFormat = FORMAT_DECIMAL;

    int outputQuantity = 1;
    bool blankLineBetweenItems = false;
    bool numberListItems = false;

    int decimalDigits = 16;
    long long integerMin = 0;
    long long integerMax = 100;
    int randomCharactersLength = 16;
    bool includeNumbers = true;
    bool includeUppercase = true;
    bool includeLowercase = true;
    bool includeSpecial = false;
    int rawDataAmount = 256;
    int rawDataUnit = RAW_DATA_UNIT_BITS;
    int rawDataEncoding = 0;
    // Indexed by CSPRNG::RawEncoding
    static constexpr int RAW_DATA_ENCODING_COUNT = 14;
    static constexpr std::array<bool, RAW_DATA_ENCODING_COUNT> DEFAULT_RAW_DATA_GROUPING_ENABLED = {false, false, true, false, false, false, false, false, false, false, false, false, false, false};
    static constexpr std::array<int, RAW_DATA_ENCODING_COUNT>  DEFAULT_RAW_DATA_GROUP_SIZE       = {4,     4,     8,    4,     4,     4,     8,     4,     4,     3,     4,     4,     4,     5};
    static constexpr std::array<bool, RAW_DATA_ENCODING_COUNT> DEFAULT_RAW_DATA_UPPERCASE        = {true,  false, false, false, false, false, true,  true,  false, true,  false, false, false, false};
    std::array<bool, RAW_DATA_ENCODING_COUNT> rawDataGroupingEnabled = DEFAULT_RAW_DATA_GROUPING_ENABLED;
    std::array<int, RAW_DATA_ENCODING_COUNT> rawDataGroupSize = DEFAULT_RAW_DATA_GROUP_SIZE;
    std::array<bool, RAW_DATA_ENCODING_COUNT> rawDataUppercase = DEFAULT_RAW_DATA_UPPERCASE;

    bool uuidUppercase = false;
    bool uuidHyphens = true;
    int uuidV7Ordering = UUIDV7_DEDICATED_COUNTER;
    int uuidV7CounterBits = 12;
    int uuidV7IncrementBits = 32;

    int passphraseWordCount = 6;
    std::vector<char> passphraseSeparator;

    std::vector<char> oneTimePadMessage;
    std::vector<char> oneTimePadFilePath;
    long long oneTimePadFileSize = 0;
    uint64_t oneTimePadFileLastWriteTime = 0;
    int oneTimePadMessageLength = 16;
    int oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;

    int oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;
    std::vector<char> oneTimePadKey;
    std::vector<char> oneTimePadCiphertext;
    std::string oneTimePadResultFileName;

    std::vector<char> generatedOutput;
    std::vector<uint8_t> generatedBinaryOutput;
    GeneratedOutputDetails generatedOutputDetails;
    std::string generatedAtText = "";
    std::string generationTimeText = "";

    std::vector<std::string> cachedWordList;

    bool truncateOutputView = true;
    volatile bool cancelGenerationRequested{false};
    std::atomic<bool> generationInCryptoPass{false};

    bool closeRequested = false;
    double closeRequestedTime = 0.0;

    bool showDataLockWarning = true;

    bool showAboutModal = false;

    bool hasReachedTargetEntropy() const {
        return collectedEntropyBits >= targetEntropyBits;
    }

    std::set<Entropy::EntropySource> enabledEntropySources() const {
        std::set<Entropy::EntropySource> enabledSources;
        if (microphoneEnabled)
            enabledSources.insert(Entropy::EntropySource::Microphone);
        if (keystrokeEnabled)
            enabledSources.insert(Entropy::EntropySource::Keystroke);
        if (clockDriftEnabled)
            enabledSources.insert(Entropy::EntropySource::ClockDrift);
        if (cpuJitterEnabled)
            enabledSources.insert(Entropy::EntropySource::CpuJitter);
        if (mouseMovementEnabled)
            enabledSources.insert(Entropy::EntropySource::Mouse);
        return enabledSources;
    }

    bool hasOneTimePadResult() const {
        return oneTimePadResultMode != ONE_TIME_PAD_NO_RESULT;
    }

    void wipeGeneratedOutput() {
        Crypto::SecureClearVector(generatedOutput);
        Crypto::SecureClearVector(generatedBinaryOutput);
        generatedOutputDetails = GeneratedOutputDetails();
    }

    void wipeOneTimePadResult() {
        Crypto::SecureClearVector(oneTimePadKey);
        Crypto::SecureClearVector(oneTimePadCiphertext);
        oneTimePadResultFileName.clear();
        oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;
    }

    ~AppState() {
        wipeGeneratedOutput();
        Crypto::SecureClearVector(oneTimePadMessage);
        Crypto::SecureClearVector(passphraseSeparator);
        wipeOneTimePadResult();
    }
};

extern AppState g_state;
