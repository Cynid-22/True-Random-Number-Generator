#include "logic.h"
#include "app_state.h"
#include <cmath>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>

// Default wordlist constants
static const int DEFAULT_WORDLIST_ENTROPY = 15; // Conservative estimate for 123565 words

// Helper to estimate entropy for passphrase
// For default wordlist: uses hardcoded conservative value (15 bits/word)
// For custom files: counts newlines, calculates log2, then applies 75% for conservative estimate
int EstimateWordListEntropy(const char* path) {
    if (!path || strlen(path) == 0) return DEFAULT_WORDLIST_ENTROPY;

    // Check if using default wordlist (compare end of path)
    std::string pathStr(path);
    if (pathStr.find("default_wordlist.txt") != std::string::npos) {
        return DEFAULT_WORDLIST_ENTROPY;
    }
    
    // For custom files, count newline characters for line count
    std::ifstream file(path);
    if (!file.is_open()) return DEFAULT_WORDLIST_ENTROPY; // Fall back to default
    
    // Don't skip whitespace so we can count newlines
    file.unsetf(std::ios_base::skipws);
    
    // Count all newline characters efficiently
    size_t lineCount = std::count(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>(),
        '\n'
    );
    file.close();
    
    if (lineCount < 2) return 1;
    
    // Apply 75% multiplier for conservative estimate
    double rawEntropy = std::log2((double)lineCount);
    return (int)std::ceil(rawEntropy * 0.75);
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
            float bitsPerWord = 14.0f; // Default conservative estimate
            
            // Only re-count if path changes or first run
            if (strcmp(g_state.cachedWordListPath, g_state.wordListPath) != 0 || g_state.cachedWordListEntropy == 0) {
                 int lines = EstimateWordListEntropy(g_state.wordListPath);
                 g_state.cachedWordListEntropy = lines;
                 strncpy(g_state.cachedWordListPath, g_state.wordListPath, sizeof(g_state.cachedWordListPath) - 1);
                 g_state.cachedWordListPath[sizeof(g_state.cachedWordListPath) - 1] = '\0';
            }
            
            if (g_state.cachedWordListEntropy > 0) {
                bitsPerWord = (float)g_state.cachedWordListEntropy;
            }
            
            bits = g_state.passphraseWordCount * bitsPerWord;
            break;
        }
        
        case 6: // One-Time Pad
        {
            if (g_state.otpInputMode == 0) { // Text Input
                // strlen returns bytes, which is what we want (1 byte message = 1 byte key needed)
                bits = strlen(g_state.otpMessage) * 8.0f;
            } else { // File Input
                bits = (float)g_state.otpFileSize * 8.0f;
            }
            break;
        }
    }
    
    // Enforce user requested minimum
    return (bits < 512.0f) ? 512.0f : bits;
}

void UpdateTargetEntropy() {
    g_state.targetBits = CalculateRequiredEntropy();
}
