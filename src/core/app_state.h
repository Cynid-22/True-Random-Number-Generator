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
#include "output_settings.h"

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

struct AppState : OutputSettings {
    AppState() {
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
    double availableEntropyBits = 0.0;
    double usedEntropyBits = 0.0;

    struct TrailPoint { float x, y; };
    std::vector<TrailPoint> mouseTrail;
    std::vector<char> keystrokePreview;

    float targetEntropyBits = AppConfig::MINIMUM_ENTROPY_BITS;

    int oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;
    std::vector<char> oneTimePadKey;
    std::vector<char> oneTimePadCiphertext;
    std::string oneTimePadResultFileName;

    std::vector<char> generatedOutput;
    std::vector<uint8_t> generatedBinaryOutput;
    GeneratedOutputDetails generatedOutputDetails;
    std::string generatedAtText = "";
    std::string generationTimeText = "";

    bool truncateOutputView = true;
    std::atomic<bool> cancelGenerationRequested{false};
    std::atomic<bool> generationInCryptoPass{false};

    bool closeRequested = false;
    double closeRequestedTime = 0.0;

    bool showUsedDataWarning = true;

    bool showAboutModal = false;

    std::string exportStatusText;
    std::string errorModalHeading;
    std::string errorModalText;
    bool showErrorModal = false;

    bool hasMinimumEntropy() const {
        return availableEntropyBits + usedEntropyBits >= AppConfig::MINIMUM_ENTROPY_BITS;
    }

    bool hasReachedTargetEntropy() const {
        return availableEntropyBits >= targetEntropyBits;
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
        wipeOneTimePadResult();
    }
};

extern AppState g_state;
