// TRNG - Application State Header
// Contains all application state and configuration

#pragma once

#include <string>

// Application state - global configuration and runtime data
struct AppState {
    // Entropy sources enabled
    bool microphoneEnabled = true;
    bool keystrokeEnabled = true;
    bool clockDriftEnabled = true;
    bool cpuJitterEnabled = true;
    bool mouseMovementEnabled = true;
    
    // Collection state
    bool isCollecting = false;
    float collectedBits = 0.0f;
    float targetBits = 512.0f;
    
    // Output configuration
    int outputFormat = 0;  // 0=Decimal, 1=Integer, 2=Binary, 3=Custom, 4=BitByte
    
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
    char wordListPath[512] = "assets/default_wordlist.txt";
    
    // Result
    std::string generatedOutput = "";
    std::string timestamp = "";
    float entropyConsumed = 0.0f;
    
    // UI state
    int currentTab = 0;
};

// Global application state
extern AppState g_state;
