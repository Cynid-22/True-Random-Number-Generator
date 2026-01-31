#include "csprng.h"
#include "logic.h"
#include "../core/app_state.h"
#include "../crypto/sha512.h"
#include "../crypto/hkdf.h"
#include "../crypto/chacha20.h"
#include "../crypto/aes.h"
#include "../logging/logger.h"
#include <cstring>
#include <cmath>
#include <set>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <windows.h> // For SecureZeroMemory

namespace CSPRNG {

//=============================================================================
// ENTROPY SERIALIZATION
//=============================================================================

std::vector<uint8_t> SerializeEntropyData(
    const std::vector<Entropy::EntropyDataPoint>& data) {
    
    // Each data point: 8 bytes timestamp + 8 bytes value = 16 bytes
    std::vector<uint8_t> result;
    result.reserve(data.size() * 16);
    
    for (const auto& point : data) {
        // Add timestamp (little-endian)
        for (int i = 0; i < 8; i++) {
            result.push_back(static_cast<uint8_t>(point.timestamp >> (i * 8)));
        }
        // Add value (little-endian)
        for (int i = 0; i < 8; i++) {
            result.push_back(static_cast<uint8_t>(point.value >> (i * 8)));
        }
    }
    
    return result;
}

//=============================================================================
// CORE GENERATION LOGIC
//=============================================================================

std::vector<uint8_t> GenerateRandomBytes(
    const std::vector<Entropy::EntropyDataPoint>& entropyData,
    size_t numBytes,
    GenerationMode& modeUsed) {
    
    // 1. Serialize ENTIRE entropy pool
    std::vector<uint8_t> entropyBytes = SerializeEntropyData(entropyData);
    
    // Log entropy pool state
    // Log entropy pool state
    Logger::Log(Logger::Level::INFO, "CSPRNG",
                "GenerateRandomBytes: %zu data points, %zu entropy bytes, requesting %zu output bytes",
                entropyData.size(), entropyBytes.size(), numBytes);
    
    // 2. Calculate input entropy bits
    float inputBits = static_cast<float>(entropyData.size()) * 2.0f;
    float outputBits = static_cast<float>(numBytes) * 8.0f;
    
    // 3. Determine Mode (for reporting purposes)
    if (inputBits >= outputBits) {
        modeUsed = GenerationMode::Consolidation; 
        Logger::Log(Logger::Level::INFO, "CSPRNG",
                    "Mode: CONSOLIDATION (TRUE RANDOMNESS) - Pool has %.0f bits for %.0f output bits",
                    inputBits, outputBits);
    } else {
        modeUsed = GenerationMode::Expansion;
        Logger::Log(Logger::Level::INFO, "CSPRNG",
                    "Mode: EXPANSION (PSEUDO-RANDOM) - Pool has %.0f bits for %.0f output bits",
                    inputBits, outputBits);
    }

    // 4. QUAD-LAYER ARCHITECTURE (Maximum Security)
    // Layer 1: ChaCha20 Masking (Stream Cipher)
    // Layer 2: Entropy Injection (Information Theoretic XOR)
    // Layer 3: AES-256 Transformation (Block Cipher)
    // Layer 4: ChaCha20 Final Whitening (Stream Cipher)
    
    //-------------------------------------------------------------------------
    // LAYER 1: ChaCha20 Masking
    //-------------------------------------------------------------------------
    // Hash pool + params to get master seed
    Crypto::SHA512::Hash masterSeed = Crypto::SHA512::Compute(entropyBytes);
    
    std::ostringstream infoStream;
    infoStream << "TRNG-L1|Len:" << numBytes << "|Fmt:" << g_state.outputFormat << "|";
    // Add format-specific params to context to ensure avalanche
    switch (g_state.outputFormat) {
        case 0: infoStream << "D:" << g_state.decimalDigits; break;
        case 1: infoStream << "I:" << g_state.integerMin << ":" << g_state.integerMax; break;
        case 2: infoStream << "B:" << g_state.binaryLength; break;
        case 3: infoStream << "C:" << g_state.customLength; break;
        case 4: infoStream << "U:" << g_state.bitByteUnit << "A:" << g_state.bitByteAmount; break;
        case 5: infoStream << "W:" << g_state.passphraseWordCount; break;
        case 6: infoStream << "O:" << g_state.otpInputMode; break;
    }
    infoStream << "|T:" << Entropy::GetNanosecondTimestamp();
    std::string infoStr = infoStream.str();
    std::vector<uint8_t> info(infoStr.begin(), infoStr.end());
    
    std::vector<uint8_t> salt; 
    std::vector<uint8_t> keyMaterial = Crypto::HKDF::DeriveKey(
        std::vector<uint8_t>(masterSeed.begin(), masterSeed.end()), salt, info, 44);
    
    Crypto::ChaCha20::Key key1;
    Crypto::ChaCha20::Nonce nonce1;
    std::copy(keyMaterial.begin(), keyMaterial.begin() + 32, key1.begin());
    std::copy(keyMaterial.begin() + 32, keyMaterial.begin() + 44, nonce1.begin());
    
    std::vector<uint8_t> stream1 = Crypto::ChaCha20::GenerateStream(key1, nonce1, numBytes);
    
    //-------------------------------------------------------------------------
    // LAYER 2: Entropy Injection (XOR Fold)
    //-------------------------------------------------------------------------
    // Fold full entropy pool into stream1
    if (!entropyBytes.empty()) {
        for (size_t i = 0; i < stream1.size(); i++) {
            stream1[i] ^= entropyBytes[i % entropyBytes.size()];
        }
    }
    // Logger::Log(Logger::Level::DEBUG, "CSPRNG", "Layer 2: Entropy Fold Complete");
    
    //-------------------------------------------------------------------------
    // LAYER 3: AES-256 Transformation
    //-------------------------------------------------------------------------
    // Use Hash(Stream1) to key AES-256 for transforming Stream1
    Crypto::SHA512::Hash s1Hash = Crypto::SHA512::Compute(stream1);
    
    std::vector<uint8_t> aesKey(s1Hash.begin(), s1Hash.begin() + 32); // 32 bytes Key
    std::vector<uint8_t> aesIV(s1Hash.begin() + 32, s1Hash.begin() + 48); // 16 bytes IV
    
    std::vector<uint8_t> stream3 = Crypto::AES256::EncryptCTR(aesKey, aesIV, stream1);
    // Logger::Log(Logger::Level::DEBUG, "CSPRNG", "Layer 3: AES-256 Transformation Complete");
    
    //-------------------------------------------------------------------------
    // LAYER 4: ChaCha20 Final Whitening
    //-------------------------------------------------------------------------
    // Use Hash(Stream3) to seed final ChaCha20 pass
    // This ensures AES bias (if any existed, which shouldn't) is washed away
    Crypto::SHA512::Hash s3Hash = Crypto::SHA512::Compute(stream3);
    
    std::vector<uint8_t> info4 = {'L', 'A', 'Y', 'E', 'R', '4'};
    std::vector<uint8_t> key4Mat = Crypto::HKDF::DeriveKey(
        std::vector<uint8_t>(s3Hash.begin(), s3Hash.end()), salt, info4, 44);
        
    Crypto::ChaCha20::Key key4;
    Crypto::ChaCha20::Nonce nonce4;
    std::copy(key4Mat.begin(), key4Mat.begin() + 32, key4.begin());
    std::copy(key4Mat.begin() + 32, key4Mat.begin() + 44, nonce4.begin());
    
    std::vector<uint8_t> result = Crypto::ChaCha20::GenerateStream(key4, nonce4, numBytes);
    // Logger::Log(Logger::Level::DEBUG, "CSPRNG", "Layer 4: Final Whitening Complete");
    
    // 7. Debug Logging REMOVED for Security
    
    // 8. Secure Cleanup
    SecureZeroMemory(entropyBytes.data(), entropyBytes.size());
    SecureZeroMemory(masterSeed.data(), masterSeed.size());
    SecureZeroMemory(keyMaterial.data(), keyMaterial.size());
    SecureZeroMemory(stream1.data(), stream1.size());
    SecureZeroMemory(s1Hash.data(), s1Hash.size());
    SecureZeroMemory(aesKey.data(), aesKey.size());
    SecureZeroMemory(stream3.data(), stream3.size());
    SecureZeroMemory(s3Hash.data(), s3Hash.size());
    SecureZeroMemory(key4Mat.data(), key4Mat.size());
    
    return result;
}

// Legacy wrappers no longer needed but kept empty/removed to avoid link errors if accessed
// We implemented the logic directly in GenerateRandomBytes
std::vector<uint8_t> ConsolidateEntropy(const std::vector<uint8_t>& entropyBytes, size_t outputBytes) {
    // Not used anymore
    return {};
}

std::vector<uint8_t> ExpandEntropy(const std::vector<uint8_t>& entropyBytes, size_t outputBytes) {
    // Not used anymore
    return {};
}

//=============================================================================
// FORMAT-SPECIFIC GENERATORS
//=============================================================================

std::string GenerateDecimal(const std::vector<uint8_t>& randomBytes, int digits) {
    if (randomBytes.empty() || digits <= 0) return "0.0";
    
    std::ostringstream oss;
    oss << "0.";
    
    // Use 2 bytes per digit for better uniformity
    // Caller should provide at least digits*2 bytes
    size_t bytesAvailable = randomBytes.size();
    
    for (int i = 0; i < digits; i++) {
        size_t offset = i * 2;
        uint16_t val;
        
        if (offset + 1 < bytesAvailable) {
            // Have 2 bytes available - use both
            val = (static_cast<uint16_t>(randomBytes[offset]) << 8) | randomBytes[offset + 1];
        } else if (offset < bytesAvailable) {
            // Only 1 byte available
            val = randomBytes[offset];
        } else {
            // No bytes available - this shouldn't happen if caller provides enough
            val = 0;
        }
        
        int digit = val % 10;
        oss << digit;
    }
    
    return oss.str();
}

std::string GenerateInteger(const std::vector<uint8_t>& randomBytes, int min, int max) {
    if (min > max) std::swap(min, max);
    if (randomBytes.empty()) return std::to_string(min);
    
    uint64_t range = static_cast<uint64_t>(max) - static_cast<uint64_t>(min) + 1;
    if (range == 0) range = 1;
    
    // Build a random number from bytes
    uint64_t randVal = 0;
    size_t bytesNeeded = (range <= 0xFF) ? 1 :
                         (range <= 0xFFFF) ? 2 :
                         (range <= 0xFFFFFFFF) ? 4 : 8;
    
    for (size_t i = 0; i < bytesNeeded && i < randomBytes.size(); i++) {
        randVal = (randVal << 8) | randomBytes[i];
    }
    
    // Use rejection-free modular reduction
    int64_t result = min + static_cast<int64_t>(randVal % range);
    
    return std::to_string(result);
}

std::string GenerateBinary(const std::vector<uint8_t>& randomBytes, int length) {
    if (randomBytes.empty() || length <= 0) return "";
    
    std::string result;
    result.reserve(length);
    
    for (int i = 0; i < length; i++) {
        size_t byteIdx = i / 8;
        int bitIdx = i % 8;
        
        if (byteIdx < randomBytes.size()) {
            result += ((randomBytes[byteIdx] >> bitIdx) & 1) ? '1' : '0';
        } else {
            result += '0';
        }
    }
    
    return result;
}

std::string GenerateCustomString(
    const std::vector<uint8_t>& randomBytes,
    int length,
    bool includeNumbers,
    bool includeUppercase,
    bool includeLowercase,
    bool includeSpecial) {
    
    // Build character set
    std::string charset;
    if (includeNumbers) charset += "0123456789";
    if (includeUppercase) charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (includeLowercase) charset += "abcdefghijklmnopqrstuvwxyz";
    if (includeSpecial) charset += "!@#$%^&*()_+-=[]{}|;':,.<>?";
    
    if (charset.empty() || randomBytes.empty() || length <= 0) return "";
    
    std::string result;
    result.reserve(length);
    
    size_t bytesAvailable = randomBytes.size();
    
    // Use 2 bytes per character for better uniformity
    for (int i = 0; i < length; i++) {
        size_t offset = i * 2;
        uint16_t val;
        
        if (offset + 1 < bytesAvailable) {
            val = (static_cast<uint16_t>(randomBytes[offset]) << 8) | randomBytes[offset + 1];
        } else if (offset < bytesAvailable) {
            val = randomBytes[offset];
        } else {
            val = 0;
        }
        
        size_t charIdx = val % charset.size();
        result += charset[charIdx];
    }
    
    return result;
}

std::string GenerateBitByte(
    const std::vector<uint8_t>& randomBytes,
    int amount,
    int unit,
    int format) {
    
    // Calculate bytes needed
    size_t bytesNeeded = (unit == 0) ? (amount + 7) / 8 : amount;
    
    if (randomBytes.size() < bytesNeeded) {
        bytesNeeded = randomBytes.size();
    }
    
    std::ostringstream oss;
    
    switch (format) {
        case 0: // Hexadecimal
            for (size_t i = 0; i < bytesNeeded; i++) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(randomBytes[i]);
            }
            break;
            
        case 1: { // Base64
            static const char* b64 = 
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            
            size_t i = 0;
            while (i < bytesNeeded) {
                uint32_t octet_a = i < bytesNeeded ? randomBytes[i++] : 0;
                uint32_t octet_b = i < bytesNeeded ? randomBytes[i++] : 0;
                uint32_t octet_c = i < bytesNeeded ? randomBytes[i++] : 0;
                
                uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
                
                oss << b64[(triple >> 18) & 0x3F];
                oss << b64[(triple >> 12) & 0x3F];
                oss << b64[(triple >> 6) & 0x3F];
                oss << b64[triple & 0x3F];
            }
            
            // Add padding if needed
            size_t mod = bytesNeeded % 3;
            if (mod == 1) {
                std::string s = oss.str();
                s.replace(s.length() - 2, 2, "==");
                return s;
            } else if (mod == 2) {
                std::string s = oss.str();
                s.replace(s.length() - 1, 1, "=");
                return s;
            }
            break;
        }
        
        case 2: // Binary
            {
                int bitsPrinted = 0;
                size_t totalBits = bytesNeeded * 8;
                for (size_t i = 0; i < bytesNeeded; i++) {
                    for (int bit = 7; bit >= 0; bit--) {
                        oss << ((randomBytes[i] >> bit) & 1);
                        bitsPrinted++;
                        
                        // Add separator if enabled, interval reached, and not the very last bit
                        if (g_state.binarySeparatorEnabled && 
                            g_state.binarySeparatorInterval > 0 &&
                            bitsPrinted % g_state.binarySeparatorInterval == 0 &&
                            bitsPrinted < totalBits) {
                            oss << ' ';
                        }
                    }
                }
            }
            break;
    }
    
    return oss.str();
}

std::string GeneratePassphrase(
    const std::vector<uint8_t>& randomBytes,
    int wordCount,
    const std::string& separator,
    const std::vector<std::string>& wordlist) {
    
    if (wordlist.empty() || randomBytes.empty() || wordCount <= 0) {
        return "[Error: Wordlist not loaded]";
    }
    
    std::string result;
    size_t bytesPerWord = 3; // 3 bytes = 24 bits, enough for ~16M words
    
    for (int i = 0; i < wordCount; i++) {
        // Build index from 3 random bytes
        size_t offset = i * bytesPerWord;
        uint32_t randomIndex = 0;
        
        for (size_t j = 0; j < bytesPerWord && (offset + j) < randomBytes.size(); j++) {
            randomIndex = (randomIndex << 8) | randomBytes[offset + j];
        }
        
        // Select word using modular arithmetic
        size_t wordIndex = randomIndex % wordlist.size();
        
        if (i > 0) result += separator;
        result += wordlist[wordIndex];
    }
    
    return result;
}

std::string GenerateOTP(
    const std::vector<uint8_t>& randomBytes,
    const std::string& message) {
    
    std::string result;
    result.reserve(message.size());
    size_t keyIdx = 0;
    
    for (char c : message) {
        // Validate ASCII (Printable 32-126)
        if (c < 32 || c > 126) {
            return "[Error: Message contains non-ASCII characters. Only printable ASCII (32-126) allowed.]";
        }
        
        // Rejection Sampling for Modulo 95 Uniformity
        // Range 95 (0-94). 256 / 95 = 2 rem 66.
        // Valid range: 0..189 (190 values = 2 * 95). Reject >= 190.
        uint8_t keyVal = 0;
        bool found = false;
        
        while (keyIdx < randomBytes.size()) {
            uint8_t b = randomBytes[keyIdx++];
            if (b < 190) {
                keyVal = b % 95;
                found = true;
                break;
            }
        }
        
        if (!found) {
            return "[Error: Insufficient entropy (rejection sampling exhausted). Please retry.]";
        }
        
        // ASCII Modulo Encryption: Cipher = (Msg + Key) % 95
        int msgVal = static_cast<int>(c) - 32;
        int cipherVal = (msgVal + static_cast<int>(keyVal)) % 95;
        result += static_cast<char>(cipherVal + 32);
    }
    
    return result;
}

std::vector<uint8_t> GenerateOTPFile(
    const std::vector<uint8_t>& randomBytes,
    const std::vector<uint8_t>& fileData) {
    
    std::vector<uint8_t> result(fileData.size());
    
    size_t keySize = randomBytes.size();
    for (size_t i = 0; i < fileData.size(); i++) {
        result[i] = fileData[i] ^ randomBytes[i % keySize];
    }
    
    return result;
}

//=============================================================================
// MAIN GENERATION FUNCTION
//=============================================================================

GenerationResult GenerateOutput() {
    GenerationResult result;
    result.success = false;
    result.mode = GenerationMode::Expansion;
    result.entropyConsumed = 0.0f;
    result.rawBytesGenerated = 0;
    
    // Get enabled sources based on checkbox state (for new data filtering)
    std::set<Entropy::EntropySource> enabledSources;
    if (g_state.microphoneEnabled) enabledSources.insert(Entropy::EntropySource::Microphone);
    if (g_state.keystrokeEnabled) enabledSources.insert(Entropy::EntropySource::Keystroke);
    if (g_state.clockDriftEnabled) enabledSources.insert(Entropy::EntropySource::ClockDrift);
    if (g_state.cpuJitterEnabled) enabledSources.insert(Entropy::EntropySource::CpuJitter);
    if (g_state.mouseMovementEnabled) enabledSources.insert(Entropy::EntropySource::Mouse);
    
    // Get pooled entropy data
    // Note: If there's locked data, we include ALL locked data plus new data from enabled sources
    std::vector<Entropy::EntropyDataPoint> pooledData;
    
    if (g_state.lockedDataTimestamp > 0) {
        // We have locked data - get everything
        pooledData = g_state.entropyPool.GetPooledData();
    } else {
        // No lock yet - respect current checkbox selections
        pooledData = g_state.entropyPool.GetPooledDataForSources(enabledSources);
    }
    
    if (pooledData.empty()) {
        result.errorMessage = "No entropy data available";
        return result;
    }
    
    // Calculate required bytes based on output format
    // IMPORTANT: Must match what the format generators actually consume!
    size_t bytesNeeded = 0;
    
    switch (g_state.outputFormat) {
        case 0: // Decimal - uses 2 bytes per digit
            bytesNeeded = g_state.decimalDigits * 2;
            break;
        case 1: // Integer - needs up to 8 bytes
            bytesNeeded = 8;
            break;
        case 2: // Binary - 1 bit per output bit, so ceil(length/8)
            bytesNeeded = (g_state.binaryLength + 7) / 8;
            break;
        case 3: // Custom - uses 2 bytes per character
            bytesNeeded = g_state.customLength * 2;
            break;
        case 4: // Bit/Byte - exact amount requested
            bytesNeeded = (g_state.bitByteUnit == 0) ? 
                         (g_state.bitByteAmount + 7) / 8 : g_state.bitByteAmount;
            break;
        case 5: // Passphrase - 3 bytes per word
            bytesNeeded = g_state.passphraseWordCount * 3;
            break;
        case 6: // OTP - 1 byte per input (Text Mode needs 2x for Rejection)
            if (g_state.otpInputMode == 0) {
                // Text mode: ASCII safe Modulo-95. 
                // We request 2x bytes to ensure Rejection Sampling < 190 succeeds.
                bytesNeeded = strlen(g_state.otpMessage) * 2;
            } else {
                // File mode: Raw XOR, 1:1 ratio
                bytesNeeded = static_cast<size_t>(g_state.otpFileSize);
            }
            break;
    }
    
    // Ensure minimum security (512 bits = 64 bytes minimum)
    if (bytesNeeded < 64) bytesNeeded = 64;
    
    // Generate random bytes
    GenerationMode mode;
    std::vector<uint8_t> randomBytes = GenerateRandomBytes(pooledData, bytesNeeded, mode);
    result.mode = mode;
    result.rawBytesGenerated = randomBytes.size();
    
    // Format output based on selected format
    switch (g_state.outputFormat) {
        case 0: // Decimal
            result.output = GenerateDecimal(randomBytes, g_state.decimalDigits);
            break;
            
        case 1: // Integer
            result.output = GenerateInteger(randomBytes, g_state.integerMin, g_state.integerMax);
            break;
            
        case 2: // Binary
            result.output = GenerateBinary(randomBytes, g_state.binaryLength);
            break;
            
        case 3: // Custom
            result.output = GenerateCustomString(
                randomBytes,
                g_state.customLength,
                g_state.includeNumbers,
                g_state.includeUppercase,
                g_state.includeLowercase,
                g_state.includeSpecial);
            break;
            
        case 4: // Bit/Byte
            result.output = GenerateBitByte(
                randomBytes,
                g_state.bitByteAmount,
                g_state.bitByteUnit,
                g_state.bitByteFormat);
            break;
            
        case 5: // Passphrase
            // Load wordlist if needed
            if (!LoadWordListForGeneration()) {
                result.errorMessage = "Failed to load wordlist";
                SecureZeroMemory(randomBytes.data(), randomBytes.size());
                return result;
            }
            result.output = GeneratePassphrase(
                randomBytes,
                g_state.passphraseWordCount,
                g_state.passphraseSeparator,
                g_state.cachedWordList);
            break;
            
        case 6: // OTP
            if (g_state.otpInputMode == 0) {
                // Text input
                result.output = GenerateOTP(randomBytes, g_state.otpMessage);
            } else {
                // File input - read file and XOR
                std::ifstream file(g_state.otpFilePath, std::ios::binary);
                if (!file.is_open()) {
                    result.errorMessage = "Failed to open input file";
                    SecureZeroMemory(randomBytes.data(), randomBytes.size());
                    return result;
                }
                
                std::vector<uint8_t> fileData(
                    (std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
                file.close();
                
                std::vector<uint8_t> encrypted = GenerateOTPFile(randomBytes, fileData);
                
                // Output as hex
                std::ostringstream oss;
                for (uint8_t b : encrypted) {
                    oss << std::hex << std::setw(2) << std::setfill('0') 
                        << static_cast<int>(b);
                }
                result.output = oss.str();
                
                // Secure cleanup
                SecureZeroMemory(fileData.data(), fileData.size());
                SecureZeroMemory(encrypted.data(), encrypted.size());
            }
            break;
    }
    
    // Calculate entropy consumed
    result.entropyConsumed = static_cast<float>(randomBytes.size()) * 8.0f;
    if (mode == GenerationMode::Consolidation) {
        // In consolidation, we consumed what we hashed
        result.entropyConsumed = std::min(result.entropyConsumed, g_state.collectedBits);
    }
    
    // Secure cleanup
    SecureZeroMemory(randomBytes.data(), randomBytes.size());
    
    result.success = true;
    Logger::Log(Logger::Level::INFO, "CSPRNG",
                "Output generated successfully. Format: %d, Mode: %s, Bytes: %zu",
                g_state.outputFormat,
                mode == GenerationMode::Consolidation ? "CONSOLIDATION" : "EXPANSION",
                result.rawBytesGenerated);
    
    return result;
}

} // namespace CSPRNG
