// TRNG - Application State Header
// Contains all application state and configuration

#pragma once

#include <string>
#include <vector>
#include "../entropy/clock_drift/clock_drift.h"

// Application state - global configuration and runtime data
struct AppState {
    // Entropy Collectors
    Entropy::ClockDriftCollector clockDriftCollector;

    // Entropy sources enabled
    bool microphoneEnabled = true;
    bool keystrokeEnabled = true;
    bool clockDriftEnabled = true;
    bool cpuJitterEnabled = true;
    bool mouseMovementEnabled = true;
    
    // Debug
    bool keepLogs = false;
    
    // Collection state
    bool isCollecting = false;
    float collectedBits = 0.0f; // Computed total of included sources
    
    // Per-source collected entropy (raw)
    float entropyMic = 0.0f;
    float entropyKeystroke = 0.0f;
    float entropyClock = 0.0f;
    float entropyJitter = 0.0f;
    float entropyMouse = 0.0f;

    float targetBits = 512.0f;
    
    // Output configuration
    int outputFormat = 0;  // 0=Decimal, 1=Integer, 2=Binary, 3=Custom, 4=BitByte, 5=Passphrase, 6=OTP
    
    // Format params
    int decimalDigits = 16;
    int integerMin = 0;
    int integerMax = 100;
    int binaryLength = 64;
    int customLength = 16;
    bool includeNumbers = true;
    bool includeUppercase = true;
    bool includeLowercase = true;
    bool includeSpecial = false;
    int bitByteAmount = 256;
    int bitByteUnit = 0;   // 0=Bits, 1=Bytes
    int bitByteFormat = 0; // 0=Hex, 1=Base64, 2=Binary
    
    // Passphrase params
    int passphraseWordCount = 6;
    char passphraseSeparator[16] = "-";
    
    // One-Time Pad params
    char otpMessage[1024 * 1024] = ""; // 1MB buffer for manual input
    char otpFilePath[512] = "";        // Path for file input
    long long otpFileSize = 0;         // Size of selected file
    int otpInputMode = 0;              // 0=Text, 1=File
    
    // Result
    std::string generatedOutput = "";
    std::string timestamp = "";
    float entropyConsumed = 0.0f;
    
    // Wordlist file content cache (loaded once, reused for generation)
    std::vector<std::string> cachedWordList;  // Parsed words for generation
    bool wordListCacheValid = false;          // Whether cache is valid
    
    // UI state
    int currentTab = 0;
    
    // Helper to check if we have enough entropy for consolidation (True Randomness)
    bool isEntropyValid() const {
        return collectedBits >= targetBits;
    }
};

// Global application state
extern AppState g_state;
