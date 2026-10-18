// TRNG - Application State Header
// Contains all application state and configuration

#pragma once

#include <string>
#include <vector>
#include <atomic>
#include "../entropy/clock_drift/clock_drift.h"
#include "../entropy/cpu_jitter/cpu_jitter.h"
#include "../entropy/keystroke/keystroke.h"
#include "../entropy/mouse/mouse.h"
#include "../entropy/microphone/microphone.h"
#include "../entropy/pool.h"
#include "../crypto/secure_mem.h"
#include "../../config/AppConfig.h"

// Application constants
// APP_VERSION is injected at compile time via -DAPP_VERSION="x.y.z"
// Falls back to "dev" for local builds without the flag.
#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

// Application state - global configuration and runtime data
struct AppState {
    AppState() {
        passphraseSeparator.assign(AppConfig::PASSPHRASE_SEPARATOR_MAX_BYTES, '\0');
        passphraseSeparator[0] = '-';
        otpMessage.assign(AppConfig::OTP_MESSAGE_MAX_BYTES, '\0');
        otpFilePath.assign(AppConfig::OTP_FILEPATH_MAX_BYTES, '\0');
    }

    // Entropy Collectors
    Entropy::ClockDriftCollector clockDriftCollector;
    Entropy::CpuJitterCollector cpuJitterCollector;
    Entropy::KeystrokeCollector keystrokeCollector;
    Entropy::MouseCollector mouseCollector;
    Entropy::MicrophoneCollector microphoneCollector;
    
    // Centralized entropy pool (stores all collected data with timestamps)
    Entropy::EntropyPool entropyPool;

    // Entropy sources enabled
    bool microphoneEnabled = true;
    bool keystrokeEnabled = true;
    bool clockDriftEnabled = true;
    bool cpuJitterEnabled = true;
    bool mouseMovementEnabled = true;
    
    // Debug
    bool keepLogs = false;
    bool loggingWasEverEnabled = false;  // Track if logging was enabled at any point during session
    
    // Collection state
    bool isCollecting = false;
    float collectedBits = 0.0f; // Total computed bits (Locked + New)
    
    // Pooling Logic State
    uint64_t lockedDataTimestamp = 0; // 0 = No lock. Data <= this timestamp is locked.
    
    // Per-source collected entropy (raw)
    float entropyMic = 0.0f;
    float entropyKeystroke = 0.0f;
    float entropyClock = 0.0f;
    float entropyJitter = 0.0f;
    float entropyMouse = 0.0f;

    // Visualization State
    struct VizPoint { float x, y; };
    std::vector<VizPoint> mouseTrail; // Stores normalized coordinates (0.0-1.0)
    std::vector<char> keystrokePreview; // Stores recent keystrokes for display (vector for secure wiping)

    float targetBits = 512.0f;
    
    // Output configuration
    // 0=Decimal, 1=Integer, 3=Custom, 4=BitByte, 5=Passphrase, 6=OTP, 7=UUIDv4, 8=UUIDv7
    // (2 was the old Binary String format, now merged into BitByte's Binary output)
    int outputFormat = 0;

    // Quantity / list params (all formats except One-Time Pad)
    int outputQuantity = 1;       // How many items to generate
    bool listBlankLine = false;   // Blank line between items (else one per line)
    bool listNumbered = false;    // Prefix each item with "1. ", "2. ", ...
    
    // Format params
    int decimalDigits = 16;
    long long integerMin = 0;
    long long integerMax = 100;
    int customLength = 16;
    bool includeNumbers = true;
    bool includeUppercase = true;
    bool includeLowercase = true;
    bool includeSpecial = false;
    int bitByteAmount = 256;
    int bitByteUnit = 0;   // 0=Bits, 1=Bytes
    int bitByteFormat = 0; // Raw Data encoding, CSPRNG::RawEncoding (0=Hex, 1=Base64, 2=Binary, ...)
    // Grouping (a space every N characters), per encoding, indexed by bitByteFormat:
    //                          Hex    B64    Bin   Oct    Dec    Duo    B32    Crock  B36    B45    B58    B62    B64URL B85
    static constexpr int kRawEncodingCount = 14;
    bool rawGroupEnabled[kRawEncodingCount] = {false, false, true, false, false, false, false, false, false, false, false, false, false, false};
    int rawGroupSize[kRawEncodingCount]     = {4,     4,     8,    4,     4,     4,     8,     4,     4,     3,     4,     4,     4,     5};
    // Uppercase letters, for encodings whose letters have one case (Hex, Duo, B32, Crock, B36, B45); lowercase by default
    bool rawUppercase[kRawEncodingCount] = {};

    // UUID params (shared by v4 and v7)
    bool uuidUppercase = false;
    bool uuidHyphens = true;
    // UUIDv7 batch ordering (RFC 9562 Section 6.2)
    int uuidV7Method = 0;            // 0=Method 1 (dedicated counter), 1=Method 2 (monotonic random), 2=None
    int uuidV7CounterBits = 12;      // Method 1: counter length (12 = all of rand_a; more borrows from rand_b)
    int uuidV7IncrementBits = 32;    // Method 2: random increment drawn from [1, 2^bits]
    
    // Passphrase params
    int passphraseWordCount = 6;
    std::vector<char> passphraseSeparator;
    
    // One-Time Pad params
    std::vector<char> otpMessage; // Buffer for manual input
    std::vector<char> otpFilePath; // Path for file input
    long long otpFileSize = 0;         // Size of selected file
    int otpInputMode = 0;              // 0=Text, 1=File
    
    // Result
    std::vector<char> generatedOutput; // Vector for secure wiping (avoid std::string heap artifacts)
    std::string timestamp = "";
    float entropyConsumed = 0.0f;
    
    // Wordlist file content cache (loaded once, reused for generation)
    std::vector<std::string> cachedWordList;  // Parsed words for generation
    bool wordListCacheValid = false;          // Whether cache is valid
    
    // UI state
    int currentTab = 0;
    bool truncateOutputView = true;
    volatile bool cancelGeneration{false};
    
    // Logging warning window state
    bool showLoggingWarningWindow = false;
    float loggingWarningCountdown = 5.0f;  // 5 second countdown
    
    // Data Lock Warning State
    bool showDataLockWarning = true;
    
    // UI Modal State
    bool showAboutModal = false;
    
    // NIST Export State
    bool isExportingNist = false;
    std::atomic<size_t> nistBytesWritten = 0;
    size_t nistTotalBytes = 0;
    std::string nistError = "";
    
    // Helper to check if we have enough entropy for consolidation (True Randomness)
    bool isEntropyValid() const {
        return collectedBits >= targetBits;
    }

    // Destructor to ensure global buffers are wiped
    ~AppState() {
        if (!generatedOutput.empty()) {
            Crypto::SecureClearVector(generatedOutput);
        }
        Crypto::SecureClearVector(otpMessage);
        Crypto::SecureClearVector(passphraseSeparator);
        // Other members like collectors have their own destructors
    }
};

// Global application state
extern AppState g_state;
