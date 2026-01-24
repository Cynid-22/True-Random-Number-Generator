#include "logic.h"
#include "../core/app_state.h"
#include <cmath>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>

// Default wordlist entropy: log2(123565) ≈ 16.9 bits per word
// We use 16.5 bits for calculation (conservative)
static const float DEFAULT_WORDLIST_ENTROPY = 16.5f;

// Load the default wordlist into memory for generation
bool LoadWordListForGeneration() {
    try {
        // If already loaded, don't reload
        if (!g_state.cachedWordList.empty()) {
            return true;
        }
        
        std::ifstream file("assets/default_wordlist.txt");
        if (!file.is_open()) return false;
        
        // Load all words
        g_state.cachedWordList.clear();
        g_state.cachedWordList.reserve(125000); // ~123565 words expected
        
        std::string line;
        while (std::getline(file, line)) {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t\r\n");
            size_t end = line.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                g_state.cachedWordList.push_back(line.substr(start, end - start + 1));
            }
        }
        
        file.close();
        g_state.wordListCacheValid = true;
        
        return !g_state.cachedWordList.empty();
    } catch (...) {
        g_state.cachedWordList.clear();
        g_state.wordListCacheValid = false;
        return false;
    }
}

float CalculateRequiredEntropy() {
    float bits = 0.0f;
    
    switch (g_state.outputFormat) {
        case 0: // Decimal
            // log2(10) per digit ~ 3.3219 bits
            bits = g_state.decimalDigits * 3.321928f;
            break;
            
        case 1: // Integer Range
        {
            long long range = (long long)g_state.integerMax - (long long)g_state.integerMin + 1;
            if (range <= 1) bits = 1.0f;
            else bits = std::ceil(std::log2((double)range));
            break;
        }
            
        case 2: // Binary
            bits = (float)g_state.binaryLength;
            break;
            
        case 3: // Custom String
        {
            int charsetSize = 0;
            if (g_state.includeNumbers) charsetSize += 10;
            if (g_state.includeUppercase) charsetSize += 26;
            if (g_state.includeLowercase) charsetSize += 26;
            if (g_state.includeSpecial) charsetSize += 32; // Approx standard special chars
            
            if (charsetSize < 1) charsetSize = 1;
            bits = g_state.customLength * std::log2((double)charsetSize);
            break;
        }
            
        case 4: // Bit/Byte
            if (g_state.bitByteUnit == 0) // Bits
                bits = (float)g_state.bitByteAmount;
            else // Bytes
                bits = g_state.bitByteAmount * 8.0f;
            break;
            
        case 5: // Passphrase
        {
            // Default wordlist: 123,565 words = ~16.5 bits per word
            bits = g_state.passphraseWordCount * DEFAULT_WORDLIST_ENTROPY;
            break;
        }
        
        case 6: // One-Time Pad
        {
            if (g_state.otpInputMode == 0) { // Text Input
                bits = strlen(g_state.otpMessage) * 8.0f;
            } else { // File Input
                bits = (float)g_state.otpFileSize * 8.0f;
            }
            break;
        }
    }
    
    // Enforce minimum of 512 bits
    return (bits < 512.0f) ? 512.0f : bits;
}

void UpdateTargetEntropy() {
    g_state.targetBits = CalculateRequiredEntropy();
}
