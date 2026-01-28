#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "sha512.h"

namespace Crypto {

// HKDF (HMAC-based Key Derivation Function) per RFC 5869
// Uses SHA-512 as the underlying hash function
class HKDF {
public:
    // Maximum output length: 255 * HashLen = 255 * 64 = 16320 bytes
    static constexpr size_t MAX_OUTPUT_LENGTH = 255 * SHA512::HASH_SIZE;
    
    // Full HKDF: Extract-then-Expand
    // ikm = Input Keying Material (entropy)
    // salt = Optional salt (can be empty)
    // info = Optional context info (can be empty)
    // length = Desired output length in bytes
    static std::vector<uint8_t> DeriveKey(
        const std::vector<uint8_t>& ikm,
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& info,
        size_t length);
    
    // Extract: Derive PRK from input keying material
    // Returns 64-byte pseudorandom key
    static SHA512::Hash Extract(
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& ikm);
    
    // Expand: Expand PRK to desired output length
    static std::vector<uint8_t> Expand(
        const SHA512::Hash& prk,
        const std::vector<uint8_t>& info,
        size_t length);
};

} // namespace Crypto
