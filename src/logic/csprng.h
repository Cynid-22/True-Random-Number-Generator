#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <set>
#include "../entropy/entropy_common.h"

namespace CSPRNG {

// Generation mode: collected entropy vs. the entropy target for the format.
// Decided once by the caller with the same rule the UI uses
// (collectedBits >= targetBits), so logs, warnings and the OTP gate agree.
enum class GenerationMode {
    Consolidation,  // Collected >= Target: TRUE RANDOMNESS
    Expansion       // Collected < Target:  PSEUDO-RANDOM (CSPRNG quality)
};

// Result of a generation operation
struct GenerationResult {
    bool success;
    std::vector<char> output;     // Formatted output (vector for secure wiping)
    std::string errorMessage;     // Error message if failed
    GenerationMode mode;          // Which mode was used
    float entropyConsumed;        // Bits of entropy consumed
    size_t rawBytesGenerated;     // Raw bytes generated before formatting
};

// Generate random bytes from the entropy pool
// This is the core function that:
// 1. Retrieves and serializes pooled entropy data
// 2. Generates the required number of random bytes
// 3. Securely wipes all temporary buffers
// `attempt` > 0 is mixed into key derivation so a retry on the same pool
// always yields different bytes (used by rejection-sampling retries).
std::vector<uint8_t> GenerateRandomBytes(
    const std::vector<Entropy::EntropyDataPoint>& entropyData,
    size_t numBytes,
    uint32_t attempt = 0);

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
std::vector<char> GenerateDecimal(const std::vector<uint8_t>& randomBytes, int digits);

// Format 1: Integer in range [min, max]
// Unbiased rejection sampling on 64-bit draws. If every draw in randomBytes is
// rejected, returns empty and sets exhausted; the caller must retry with fresh
// bytes (see GenerateOutput).
std::vector<char> GenerateInteger(const std::vector<uint8_t>& randomBytes, int64_t min, int64_t max, bool& exhausted);

// Format 3: Custom string from character set
std::vector<char> GenerateCustomString(
    const std::vector<uint8_t>& randomBytes,
    int length,
    bool includeNumbers,
    bool includeUppercase,
    bool includeLowercase,
    bool includeSpecial);

// Format 4: Bit/Byte output (hex, base64, or binary)
// Binary output prints exactly `amount` bits; hex/base64 round up to whole bytes.
std::vector<char> GenerateBitByte(
    const std::vector<uint8_t>& randomBytes,
    int amount,
    int unit,    // 0=Bits, 1=Bytes
    int format); // 0=Hex, 1=Base64, 2=Binary

// Format 5: Passphrase from wordlist
std::vector<char> GeneratePassphrase(
    const std::vector<uint8_t>& randomBytes,
    int wordCount,
    const std::string& separator,
    const std::vector<std::string>& wordlist);

// Format 6: One-Time Pad (XOR with message)
std::vector<char> GenerateOTP(
    const std::vector<uint8_t>& randomBytes,
    const std::string& message);

// Format 6: One-Time Pad for file (returns raw bytes)
std::vector<uint8_t> GenerateOTPFile(
    const std::vector<uint8_t>& randomBytes,
    const std::vector<uint8_t>& fileData);

// Format 7: UUID version 4 (RFC 9562), one per line
// Uses 16 bytes per UUID: 122 random bits + 4 version bits + 2 variant bits
std::vector<char> GenerateUUIDv4(
    const std::vector<uint8_t>& randomBytes,
    int count,
    bool uppercase,
    bool hyphens);

//=============================================================================
// MAIN GENERATION FUNCTION
//=============================================================================

// Main entry point: Generate output based on current app state
// `mode` is the TRUE RANDOMNESS / PSEUDO-RANDOM decision the UI made when
// Generate was clicked; it is reported as-is.
// Returns a GenerationResult with the formatted output
GenerationResult GenerateOutput(GenerationMode mode);

// Generate raw binary data file for NIST SP 800-22 testing
// Bypasses GUI formatting and writes directly to disk in chunks
// Updates g_state.nistBytesWritten during generation
void GenerateNistData(const std::string& filepath, size_t totalBytes);

} // namespace CSPRNG
