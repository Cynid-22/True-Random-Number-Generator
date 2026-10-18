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

// Raw Data encodings (AppState::bitByteFormat). Ids 0-2 keep their original
// meaning; the GUI lists them in base order.
enum RawEncoding : int {
    RAW_HEX = 0,          // Base 16, 0-9 a-f, whole bytes
    RAW_BASE64 = 1,       // RFC 4648, '=' padded, whole bytes
    RAW_BINARY = 2,       // Base 2, exact bits
    RAW_OCTAL = 3,        // Base 8, 3 bits per digit
    RAW_DECIMAL = 4,      // Base 10, uniform random digits
    RAW_DUODECIMAL = 5,   // Base 12 (0-9 A B), uniform random digits
    RAW_BASE32 = 6,       // RFC 4648, '=' padded, whole bytes
    RAW_CROCKFORD32 = 7,  // Crockford Base32, 5 bits per character
    RAW_BASE36 = 8,       // 0-9 A-Z, uniform random digits
    RAW_BASE45 = 9,       // RFC 9285, whole bytes
    RAW_BASE58 = 10,      // Bitcoin alphabet, uniform random digits
    RAW_BASE62 = 11,      // 0-9 A-Z a-z, uniform random digits
    RAW_BASE64URL = 12,   // RFC 4648 Section 5, unpadded, whole bytes
    RAW_BASE85 = 13,      // RFC 1924 / Git alphabet, 4 bytes -> 5 chars
    RAW_ENCODING_COUNT = 14
};

// Size of one Raw Data item for `bits` of requested data
struct RawLayout {
    uint64_t chars;       // Output characters, padding included, before grouping
    uint64_t bytes;       // Random bytes the item consumes
    double bitsCarried;   // Random bits the output actually holds (>= bits)
    bool digits;          // Uniform random digits (non power-of-two base)
};
RawLayout GetRawLayout(int encoding, uint64_t bits);

// True when the encoding's letters come in one case only (Hex, Base 12,
// Base 32, Crockford, Base 36, Base 45), so it can print them either way
bool RawHasSingleCaseLetters(int encoding);

// Format 4: Raw Data in the given encoding (see RawEncoding)
// Exact-bit encodings (Binary, Octal, Crockford) print just enough
// characters for `amount`; byte encodings round up to whole bytes; digit
// bases print enough uniform digits to hold at least `amount` bits.
// groupSize > 0 puts a space every groupSize characters. For single-case
// encodings (see RawHasSingleCaseLetters), letters are lowercase unless
// `uppercase`; other encodings ignore it.
std::vector<char> GenerateBitByte(
    const std::vector<uint8_t>& randomBytes,
    int amount,
    int unit,        // 0=Bits, 1=Bytes
    int encoding,    // RawEncoding
    int groupSize,   // 0 = no grouping
    bool uppercase);

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

// Format 7: one UUID version 4 (RFC 9562)
// Uses 16 bytes: 122 random bits + 4 version bits + 2 variant bits
std::vector<char> GenerateUUIDv4(
    const std::vector<uint8_t>& randomBytes,
    bool uppercase,
    bool hyphens);

// How UUIDv7s created in the same millisecond are ordered
// (RFC 9562 Section 6.2)
struct UUIDv7Options {
    int method = 0;            // 0=Method 1 (dedicated counter), 1=Method 2 (monotonic random), 2=None
    int counterBits = 12;      // Method 1: counter length, leftmost bits of rand_a then rand_b
    int incrementBits = 32;    // Method 2: random increment drawn from [1, 2^bits]
};

// Timestamp/counter carried between UUIDv7s of one batch so they sort in
// creation order
struct UUIDv7State {
    uint64_t lastMs = 0;
    bool haveLast = false;
    uint64_t counter = 0;      // Method 1
    uint64_t valueHi = 0;      // Method 2: 74-bit value, top 12 bits (rand_a)
    uint64_t valueLo = 0;      // Method 2: low 62 bits (rand_b)
};

// Format 8: one UUID version 7 (RFC 9562)
// 48-bit Unix millisecond timestamp + 74 bits (rand_a, rand_b) filled per
// options. Uses 16 random bytes. Pass the same state for every UUID in a
// batch.
std::vector<char> GenerateUUIDv7(
    const std::vector<uint8_t>& randomBytes,
    bool uppercase,
    bool hyphens,
    const UUIDv7Options& options,
    UUIDv7State& state);

//=============================================================================
// MAIN GENERATION FUNCTION
//=============================================================================

// Main entry point: Generate output based on current app state
// For every format except One-Time Pad, generates outputQuantity items, one
// per line (optionally blank-line separated and/or numbered "1. ", "2. ").
// `mode` is the TRUE RANDOMNESS / PSEUDO-RANDOM decision the UI made when
// Generate was clicked; it is reported as-is.
// Returns a GenerationResult with the formatted output
GenerationResult GenerateOutput(GenerationMode mode);

// Generate raw binary data file for NIST SP 800-22 testing
// Bypasses GUI formatting and writes directly to disk in chunks
// Updates g_state.nistBytesWritten during generation
void GenerateNistData(const std::string& filepath, size_t totalBytes);

} // namespace CSPRNG
