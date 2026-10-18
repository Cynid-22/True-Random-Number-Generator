#pragma once
#include <vector>
#include <cstdint>
#include <array>

namespace Crypto {

class AES256 {
public:
    static std::vector<uint8_t> EncryptCTR(
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& iv,
        const std::vector<uint8_t>& input);

private:
    static void EncryptBlock(const uint8_t* input, uint8_t* output, const uint32_t* roundKeys);

    static void ExpandKey(const uint8_t* key, uint32_t* roundKeys);

    static void SubBytes(uint8_t* state);
    static void ShiftRows(uint8_t* state);
    static void MixColumns(uint8_t* state);
    static void AddRoundKey(uint8_t* state, const uint32_t* roundKeys, int round);
};

}
