#pragma once
#include <vector>
#include <cstdint>
#include <array>

namespace Crypto {

class AES256 {
public:
    using Key = std::array<uint8_t, 32>; // 256-bit key
    using Block = std::array<uint8_t, 16>; // 128-bit block
    using IV = std::array<uint8_t, 16>; // 128-bit IV/Nonce

    // Encrypt a buffer using AES-256-CTR mode
    // key: 32 bytes (256 bits)
    // iv: 16 bytes (128 bits) initial counter
    // input: data to encrypt
    // output: encrypted data (same size as input)
    static std::vector<uint8_t> EncryptCTR(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv,
        const std::vector<uint8_t>& input);

private:
    // Core AES-256 Encryption of a single block
    static void EncryptBlock(const uint8_t* in, uint8_t* out, const uint32_t* roundKeys);

    // Key Expansion: Generates 60 32-bit round keys from 32-byte key
    static void KeyExpansion(const uint8_t* key, uint32_t* roundKeys);

    // Helper operations
    static void SubBytes(uint8_t* state);
    static void ShiftRows(uint8_t* state);
    static void MixColumns(uint8_t* state);
    static void AddRoundKey(uint8_t* state, const uint32_t* roundKeys, int round);
};

} // namespace Crypto
