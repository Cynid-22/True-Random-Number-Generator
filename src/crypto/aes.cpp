#include "aes.h"
#include <cstring>
#include <windows.h> // For SecureZeroMemory

namespace Crypto {

// S-box transformation table
static const uint8_t sbox[256] = {
    // 0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
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

// Round constant word array
static const uint8_t rcon[15] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8, 0xab, 0x4d, 0x9a // Extended range
};

// RotWord: Rotate a 4-byte word
static inline uint32_t RotWord(uint32_t word) {
    return (word << 8) | (word >> 24);
}

// SubWord: Apply S-box to 4-byte word
static inline uint32_t SubWord(uint32_t word) {
    uint32_t result = 0;
    result |= (uint32_t)sbox[(word >> 24) & 0xFF] << 24;
    result |= (uint32_t)sbox[(word >> 16) & 0xFF] << 16;
    result |= (uint32_t)sbox[(word >> 8) & 0xFF] << 8;
    result |= (uint32_t)sbox[word & 0xFF];
    return result;
}

void AES256::SubBytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

void AES256::ShiftRows(uint8_t* state) {
    uint8_t tmp;
    
    // Row 1: Shift left by 1
    tmp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = tmp;
    
    // Row 2: Shift left by 2
    tmp = state[2];
    state[2] = state[10];
    state[10] = tmp;
    tmp = state[6];
    state[6] = state[14];
    state[14] = tmp;
    
    // Row 3: Shift left by 3
    tmp = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = state[3];
    state[3] = tmp;
}

// GF(2^8) Multiplication by 2
static inline uint8_t xtime(uint8_t x) {
    return (x << 1) ^ ((x >> 7) * 0x1b);
}

// FIPS 197 §5.1.3 — MixColumns
// State uses linear byte layout: state[i*4 + j] where i=column, j=row.
// VERIFY: Run FIPS 197 Appendix B (AES-128) and Appendix C.3 (AES-256) test vectors
// to confirm correctness of column ordering.
void AES256::MixColumns(uint8_t* state) {
    uint8_t tmp[16];
    for (int i = 0; i < 4; i++) {
        int idx = i * 4;
        uint8_t a = state[idx];
        uint8_t b = state[idx+1];
        uint8_t c = state[idx+2];
        uint8_t d = state[idx+3];
        
        // FIPS 197 MixColumns formula:
        // s'[0] = 2*a + 3*b + c + d
        // s'[1] = a + 2*b + 3*c + d
        // s'[2] = a + b + 2*c + 3*d
        // s'[3] = 3*a + b + c + 2*d
        // Note: 3*x = xtime(x) ^ x
        tmp[idx]   = xtime(a) ^ (xtime(b) ^ b) ^ c ^ d;
        tmp[idx+1] = a ^ xtime(b) ^ (xtime(c) ^ c) ^ d;
        tmp[idx+2] = a ^ b ^ xtime(c) ^ (xtime(d) ^ d);
        tmp[idx+3] = (xtime(a) ^ a) ^ b ^ c ^ xtime(d);
    }
    memcpy(state, tmp, 16);
}

void AES256::AddRoundKey(uint8_t* state, const uint32_t* roundKeys, int round) {
    const uint8_t* rk = reinterpret_cast<const uint8_t*>(roundKeys + round * 4);
    for (int i = 0; i < 16; i++) {
        // Round keys are generated as big-endian words, but system is likely little-endian.
        // However, standard AES implementations usually treat state and keys as bytes.
        // Let's rely on KeyExpansion to put bytes in correct order if we access as bytes,
        // OR fix endianness here.
        // FIPS 197 defines key expansion producing words.
        // Let's access roundKeys via byte pointer assuming host endianness matches word construction.
        // Actually, safer to extract bytes from the uint32_t words manually.
        
        int wordIdx = i / 4;
        int byteIdx = 3 - (i % 4); // Big-endian word to byte
        uint32_t word = roundKeys[round * 4 + wordIdx];
        uint8_t k = (word >> (byteIdx * 8)) & 0xFF;
        
        state[i] ^= k;
    }
}

void AES256::KeyExpansion(const uint8_t* key, uint32_t* roundKeys) {
    // Nk=8 (256 bits), Nb=4 (128 bits), Nr=14
    const int Nk = 8;
    const int Nb = 4;
    const int Nr = 14;
    
    // The first Nk words are the key itself
    for (int i = 0; i < Nk; i++) {
        roundKeys[i] = (key[4*i] << 24) | (key[4*i+1] << 16) | (key[4*i+2] << 8) | key[4*i+3];
    }
    
    for (int i = Nk; i < Nb * (Nr + 1); i++) {
        uint32_t temp = roundKeys[i - 1];
        if (i % Nk == 0) {
            temp = SubWord(RotWord(temp)) ^ (static_cast<uint32_t>(rcon[(i/Nk)-1]) << 24);
        } else if (i % Nk == 4) {
            temp = SubWord(temp);
        }
        roundKeys[i] = roundKeys[i - Nk] ^ temp;
    }
}

void AES256::EncryptBlock(const uint8_t* in, uint8_t* out, const uint32_t* roundKeys) {
    uint8_t state[16];
    // Copy input to state (column major order according to spec, but usually 1:1 mapping for array)
    // FIPS 197: "State array... input bytes in0, in1... map to s00, s10, s20, s30..."
    // which corresponds to state[0], state[1]... so direct copy is fine.
    memcpy(state, in, 16);
    
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
    
    memcpy(out, state, 16);
}

std::vector<uint8_t> AES256::EncryptCTR(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& iv,
    const std::vector<uint8_t>& input) {
    
    if (key.size() != 32 || iv.size() != 16) {
        return {}; // Error
    }
    
    // Key Expansion (60 words for 14 rounds + 1)
    uint32_t roundKeys[60];
    KeyExpansion(key.data(), roundKeys);
    
    std::vector<uint8_t> output(input.size());
    uint8_t counterBuf[16];
    uint8_t keystream[16];
    memcpy(counterBuf, iv.data(), 16);
    
    size_t numBlocks = (input.size() + 15) / 16;
    
    for (size_t b = 0; b < numBlocks; b++) {
        // Encrypt counter
        EncryptBlock(counterBuf, keystream, roundKeys);
        
        // XOR with input
        size_t blockOffset = b * 16;
        size_t bytesToProc = std::min((size_t)16, input.size() - blockOffset);
        
        for (size_t i = 0; i < bytesToProc; i++) {
            output[blockOffset + i] = input[blockOffset + i] ^ keystream[i];
        }
        
        // Increment counter (big-endian 128-bit integer)
        for (int i = 15; i >= 0; i--) {
            if (++counterBuf[i] != 0) break;
        }
    }
    
    // Secure cleanup
    SecureZeroMemory(roundKeys, sizeof(roundKeys));
    SecureZeroMemory(keystream, sizeof(keystream));
    SecureZeroMemory(counterBuf, sizeof(counterBuf));
    
    return output;
}

} // namespace Crypto
