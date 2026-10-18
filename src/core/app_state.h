#pragma once

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

struct AppState {
    AppState() {
        passphraseSeparator.assign(AppConfig::PASSPHRASE_SEPARATOR_MAX_BYTES, '\0');
        passphraseSeparator[0] = '-';
        oneTimePadMessage.assign(AppConfig::ONE_TIME_PAD_MESSAGE_MAX_BYTES, '\0');
        oneTimePadFilePath.assign(AppConfig::ONE_TIME_PAD_FILE_PATH_MAX_BYTES, '\0');
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

    uint64_t lockedDataTimestamp = 0;

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
    bool rawDataGroupingEnabled[RAW_DATA_ENCODING_COUNT] = {false, false, true, false, false, false, false, false, false, false, false, false, false, false};
    int rawDataGroupSize[RAW_DATA_ENCODING_COUNT]        = {4,     4,     8,    4,     4,     4,     8,     4,     4,     3,     4,     4,     4,     5};
    bool rawDataUppercase[RAW_DATA_ENCODING_COUNT] = {};

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
    int oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;

    std::vector<char> generatedOutput;
    std::string generatedAtText = "";

    std::vector<std::string> cachedWordList;

    bool truncateOutputView = true;
    volatile bool cancelGenerationRequested{false};

    bool showDataLockWarning = true;

    bool showAboutModal = false;

    bool hasReachedTargetEntropy() const {
        return collectedEntropyBits >= targetEntropyBits;
    }

    ~AppState() {
        if (!generatedOutput.empty()) {
            Crypto::SecureClearVector(generatedOutput);
        }
        Crypto::SecureClearVector(oneTimePadMessage);
        Crypto::SecureClearVector(passphraseSeparator);
    }
};

extern AppState g_state;
