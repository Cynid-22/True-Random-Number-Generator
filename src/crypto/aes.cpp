#include "aes.h"
#include <algorithm>
#include <cstring>
#include <windows.h>

namespace Crypto {

static const uint8_t SBOX[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t ROUND_CONSTANTS[15] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a
};

static inline uint32_t RotateWord(uint32_t word) {
    return (word << 8) | (word >> 24);
}

static inline uint32_t SubstituteWord(uint32_t word) {
    uint32_t result = 0;
    result |= (uint32_t)SBOX[(word >> 24) & 0xFF] << 24;
    result |= (uint32_t)SBOX[(word >> 16) & 0xFF] << 16;
    result |= (uint32_t)SBOX[(word >> 8) & 0xFF] << 8;
    result |= (uint32_t)SBOX[word & 0xFF];
    return result;
}

void AES256::SubBytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = SBOX[state[i]];
    }
}

void AES256::ShiftRows(uint8_t* state) {
    uint8_t temp;

    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;

    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;

    temp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = temp;
}

static inline uint8_t MultiplyByTwo(uint8_t x) {
    return (x << 1) ^ ((x >> 7) * 0x1b);
}

void AES256::MixColumns(uint8_t* state) {
    uint8_t mixed[16];
    for (int column = 0; column < 4; column++) {
        int offset = column * 4;
        uint8_t a = state[offset];
        uint8_t b = state[offset + 1];
        uint8_t c = state[offset + 2];
        uint8_t d = state[offset + 3];

        mixed[offset]     = MultiplyByTwo(a) ^ (MultiplyByTwo(b) ^ b) ^ c ^ d;
        mixed[offset + 1] = a ^ MultiplyByTwo(b) ^ (MultiplyByTwo(c) ^ c) ^ d;
        mixed[offset + 2] = a ^ b ^ MultiplyByTwo(c) ^ (MultiplyByTwo(d) ^ d);
        mixed[offset + 3] = (MultiplyByTwo(a) ^ a) ^ b ^ c ^ MultiplyByTwo(d);
    }
    memcpy(state, mixed, 16);
}

void AES256::AddRoundKey(uint8_t* state, const uint32_t* roundKeys, int round) {
    for (int i = 0; i < 16; i++) {
        int wordIndex = i / 4;
        int byteShift = 3 - (i % 4);
        uint32_t keyWord = roundKeys[round * 4 + wordIndex];
        uint8_t keyByte = (keyWord >> (byteShift * 8)) & 0xFF;

        state[i] ^= keyByte;
    }
}

void AES256::ExpandKey(const uint8_t* key, uint32_t* roundKeys) {
    const int KEY_WORDS = 8;
    const int BLOCK_WORDS = 4;
    const int ROUND_COUNT = 14;

    for (int i = 0; i < KEY_WORDS; i++) {
        roundKeys[i] = (key[4*i] << 24) | (key[4*i+1] << 16) | (key[4*i+2] << 8) | key[4*i+3];
    }

    for (int i = KEY_WORDS; i < BLOCK_WORDS * (ROUND_COUNT + 1); i++) {
        uint32_t temp = roundKeys[i - 1];
        if (i % KEY_WORDS == 0) {
            temp = SubstituteWord(RotateWord(temp)) ^ (static_cast<uint32_t>(ROUND_CONSTANTS[(i/KEY_WORDS)-1]) << 24);
        } else if (i % KEY_WORDS == 4) {
            temp = SubstituteWord(temp);
        }
        roundKeys[i] = roundKeys[i - KEY_WORDS] ^ temp;
    }
}

void AES256::EncryptBlock(const uint8_t* input, uint8_t* output, const uint32_t* roundKeys) {
    uint8_t state[16];
    memcpy(state, input, 16);

    AddRoundKey(state, roundKeys, 0);

    for (int round = 1; round < 14; round++) {
        SubBytes(state);
        ShiftRows(state);
        MixColumns(state);
        AddRoundKey(state, roundKeys, round);
    }

    SubBytes(state);
    ShiftRows(state);
    AddRoundKey(state, roundKeys, 14);

    memcpy(output, state, 16);
}

std::vector<uint8_t> AES256::EncryptCTR(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& input) {

    if (key.size() != 32 || iv.size() != 16) {
        return {};
    }

    uint32_t roundKeys[60];
    ExpandKey(key.data(), roundKeys);

    std::vector<uint8_t> output(input.size());
    uint8_t counterBlock[16];
    uint8_t keystream[16];
    memcpy(counterBlock, iv.data(), 16);

    size_t blockCount = (input.size() + 15) / 16;

    for (size_t blockIndex = 0; blockIndex < blockCount; blockIndex++) {
        EncryptBlock(counterBlock, keystream, roundKeys);

        size_t blockOffset = blockIndex * 16;
        size_t bytesInBlock = std::min((size_t)16, input.size() - blockOffset);

        for (size_t i = 0; i < bytesInBlock; i++) {
            output[blockOffset + i] = input[blockOffset + i] ^ keystream[i];
        }

        for (int i = 15; i >= 0; i--) {
            if (++counterBlock[i] != 0) break;
        }
    }

    SecureZeroMemory(roundKeys, sizeof(roundKeys));
    SecureZeroMemory(keystream, sizeof(keystream));
    SecureZeroMemory(counterBlock, sizeof(counterBlock));

    return output;
}

}
