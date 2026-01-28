#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <set>
#include "../entropy/entropy_common.h"

namespace CSPRNG {

// Generation mode (based on entropy available vs. required)
enum class GenerationMode {
    Consolidation,  // Input >= Output: TRUE RANDOMNESS
    Expansion       // Input < Output:  PSEUDO-RANDOM (CSPRNG quality)
};

// Result of a generation operation
struct GenerationResult {
    bool success;
    std::string output;           // Formatted output string
    std::string errorMessage;     // Error message if failed
    GenerationMode mode;          // Which mode was used
    float entropyConsumed;        // Bits of entropy consumed
    size_t rawBytesGenerated;     // Raw bytes generated before formatting
};

// Generate random bytes from the entropy pool
// This is the core function that:
// 1. Retrieves and serializes pooled entropy data
// 2. Chooses consolidation or expansion based on input/output ratio
// 3. Generates the required number of random bytes
// 4. Securely wipes all temporary buffers
std::vector<uint8_t> GenerateRandomBytes(
    const std::vector<Entropy::EntropyDataPoint>& entropyData,
    size_t numBytes,
    GenerationMode& modeUsed);

// Consolidation mode: Hash chunks (TRUE RANDOMNESS)
// Used when pooled entropy >= required output
// Preserves information-theoretic security
std::vector<uint8_t> ConsolidateEntropy(
    const std::vector<uint8_t>& entropyBytes,
    size_t outputBytes);

// Expansion mode: HKDF + ChaCha20 (PSEUDO-RANDOM)
// Used when pooled entropy < required output
// Provides computational security
std::vector<uint8_t> ExpandEntropy(
    const std::vector<uint8_t>& entropyBytes,
    size_t outputBytes);

// Serialize entropy data points to bytes for processing
std::vector<uint8_t> SerializeEntropyData(
    const std::vector<Entropy::EntropyDataPoint>& data);

//=============================================================================
// FORMAT-SPECIFIC GENERATORS
//=============================================================================

// Format 0: Decimal number (0.0 - 1.0)
std::string GenerateDecimal(const std::vector<uint8_t>& randomBytes, int digits);

// Format 1: Integer in range [min, max]
std::string GenerateInteger(const std::vector<uint8_t>& randomBytes, int min, int max);

// Format 2: Binary string of specified length
std::string GenerateBinary(const std::vector<uint8_t>& randomBytes, int length);

// Format 3: Custom string from character set
std::string GenerateCustomString(
    const std::vector<uint8_t>& randomBytes,
    int length,
    bool includeNumbers,
    bool includeUppercase,
    bool includeLowercase,
    bool includeSpecial);

// Format 4: Bit/Byte output (hex, base64, or binary)
std::string GenerateBitByte(
    const std::vector<uint8_t>& randomBytes,
    int amount,
    int unit,    // 0=Bits, 1=Bytes
    int format); // 0=Hex, 1=Base64, 2=Binary

// Format 5: Passphrase from wordlist
std::string GeneratePassphrase(
    const std::vector<uint8_t>& randomBytes,
    int wordCount,
    const std::string& separator,
    const std::vector<std::string>& wordlist);

// Format 6: One-Time Pad (XOR with message)
std::string GenerateOTP(
    const std::vector<uint8_t>& randomBytes,
    const std::string& message);

// Format 6: One-Time Pad for file (returns raw bytes)
std::vector<uint8_t> GenerateOTPFile(
    const std::vector<uint8_t>& randomBytes,
    const std::vector<uint8_t>& fileData);

//=============================================================================
// MAIN GENERATION FUNCTION
//=============================================================================

// Main entry point: Generate output based on current app state
// Returns a GenerationResult with the formatted output
GenerationResult GenerateOutput();

} // namespace CSPRNG
