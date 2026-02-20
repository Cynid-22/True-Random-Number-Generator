#include "logic.h"
#include "../logging/logger.h"
#include "../core/app_state.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include <windows.h> // For SecureZeroMemory

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
        
        // Try multiple paths for the wordlist
        std::vector<std::string> paths = {
            "assets/default_wordlist.txt",
            "./assets/default_wordlist.txt",
            "../assets/default_wordlist.txt",
            "default_wordlist.txt"
        };
        
        // Also try to get exe directory path
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH) > 0) {
            std::string exeDir = exePath;
            size_t lastSlash = exeDir.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                exeDir = exeDir.substr(0, lastSlash + 1);
                paths.insert(paths.begin(), exeDir + "assets/default_wordlist.txt");
                paths.insert(paths.begin(), exeDir + "assets\\default_wordlist.txt");
            }
        }
        
        std::ifstream file;
        for (const auto& path : paths) {
            file.open(path);
            if (file.is_open()) {
                Logger::Log(Logger::Level::INFO, "Logic", 
                            "Loaded wordlist from: %s", path.c_str());
                break;
            }
        }
        
        if (!file.is_open()) {
            Logger::Log(Logger::Level::ERR, "Logic", 
                        "Failed to find wordlist in any path");
            return false;
        }
        
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
        
        Logger::Log(Logger::Level::INFO, "Logic", 
                    "Loaded %zu words into wordlist cache", 
                    g_state.cachedWordList.size());
        
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

// Calculates Shannon Entropy (bits per symbol)
float CalculateShannonEntropy(const std::vector<uint8_t>& data) {
    if (data.empty()) return 0.0f;
    
    // 1. Build Histogram
    std::map<uint8_t, size_t> counts;
    for (uint8_t byte : data) {
        counts[byte]++;
    }
    
    // 2. Calculate Entropy: H = -Sum(p * log2(p))
    float entropy = 0.0f;
    float total = (float)data.size();
    
    for (const auto& pair : counts) {
        float p = pair.second / total;
        entropy -= p * std::log2(p);
    }
    
    return entropy;
}

float CalculateEntropyFromDeltas(const std::vector<uint64_t>& deltas) {
    if (deltas.empty()) return 0.0f;
    
    // Convert deltas to raw bytes for analysis.
    // Use 2 bytes per delta (lower 16 bits) to capture the entropy range
    // that clock drift (16-bit mask) and other sources provide.
    // Previously only the LSB was used, discarding significant entropy.
    std::vector<uint8_t> bytes;
    bytes.reserve(deltas.size() * 2);
    
    size_t validSamples = 0;
    for (uint64_t d : deltas) {
        if (d != 0) {
            validSamples++;
            bytes.push_back(static_cast<uint8_t>(d & 0xFF));         // Low byte
            bytes.push_back(static_cast<uint8_t>((d >> 8) & 0xFF));  // High byte
        }
    }
    
    if (validSamples == 0) return 0.0f;
    
    // Calculate entropy per byte (0.0 - 8.0 bits) across the byte stream
    float bitsPerByte = CalculateShannonEntropy(bytes);
    
    // Total entropy = bits_per_byte * total_bytes_analyzed
    return bitsPerByte * (float)bytes.size();
}

std::vector<Entropy::EntropyDataPoint> GetPooledEntropyForOutput(const std::set<Entropy::EntropySource>& includedSources) {
    // Get all pooled data filtered by included sources
    // Note: This uses ALL data currently in the pool, regardless of current checkbox state
    // Data was collected when sources were enabled, so it stays in the pool
    return g_state.entropyPool.GetPooledDataForSources(includedSources);
}

bool PrepareConsolidation() {
    // Consolidation: Input >= Output
    // We have enough entropy to condense raw data into perfect random bits
    // This enables Information Theoretic Security (for OTP)
    float pooledBits = g_state.entropyPool.GetTotalBits();
    return pooledBits >= g_state.targetBits;
}

bool PrepareExpansion() {
    // Expansion: Input < Output
    // We need to use CSPRNG (HKDF + ChaCha20) to expand the key
    // Computationally Secure, but not 'True' Random for OTP
    float pooledBits = g_state.entropyPool.GetTotalBits();
    return pooledBits < g_state.targetBits;
}
