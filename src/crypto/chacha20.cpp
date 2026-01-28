#include "chacha20.h"
#include <cstring>
#include <windows.h> // For SecureZeroMemory

namespace Crypto {

// ChaCha20 constants: "expand 32-byte k" in little-endian
static const uint32_t CONSTANTS[4] = {
    0x61707865, // "expa"
    0x3320646e, // "nd 3"
    0x79622d32, // "2-by"
    0x6b206574  // "te k"
};

inline void ChaCha20::QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = ROTL(d, 16);
    c += d; b ^= c; b = ROTL(b, 12);
    a += b; d ^= a; d = ROTL(d, 8);
    c += d; b ^= c; b = ROTL(b, 7);
}

ChaCha20::State ChaCha20::InitState(const Key& key, const Nonce& nonce, uint32_t counter) {
    State state;
    
    // Words 0-3: Constants
    state[0] = CONSTANTS[0];
    state[1] = CONSTANTS[1];
    state[2] = CONSTANTS[2];
    state[3] = CONSTANTS[3];
    
    // Words 4-11: Key (little-endian)
    for (int i = 0; i < 8; i++) {
        state[4 + i] = ((uint32_t)key[i * 4 + 0]) |
                       ((uint32_t)key[i * 4 + 1] << 8) |
                       ((uint32_t)key[i * 4 + 2] << 16) |
                       ((uint32_t)key[i * 4 + 3] << 24);
    }
    
    // Word 12: Counter
    state[12] = counter;
    
    // Words 13-15: Nonce (little-endian)
    for (int i = 0; i < 3; i++) {
        state[13 + i] = ((uint32_t)nonce[i * 4 + 0]) |
                        ((uint32_t)nonce[i * 4 + 1] << 8) |
                        ((uint32_t)nonce[i * 4 + 2] << 16) |
                        ((uint32_t)nonce[i * 4 + 3] << 24);
    }
    
    return state;
}

void ChaCha20::Block(State& output, const State& input) {
    // Copy input to working state
    State working = input;
    
    // 20 rounds (10 column rounds + 10 diagonal rounds)
    for (int i = 0; i < 10; i++) {
        // Column rounds
        QuarterRound(working[0], working[4], working[8],  working[12]);
        QuarterRound(working[1], working[5], working[9],  working[13]);
        QuarterRound(working[2], working[6], working[10], working[14]);
        QuarterRound(working[3], working[7], working[11], working[15]);
        
        // Diagonal rounds
        QuarterRound(working[0], working[5], working[10], working[15]);
        QuarterRound(working[1], working[6], working[11], working[12]);
        QuarterRound(working[2], working[7], working[8],  working[13]);
        QuarterRound(working[3], working[4], working[9],  working[14]);
    }
    
    // Add original state (makes it non-invertible)
    for (int i = 0; i < 16; i++) {
        output[i] = working[i] + input[i];
    }
    
    // Secure cleanup
    SecureZeroMemory(&working, sizeof(working));
}

std::vector<uint8_t> ChaCha20::GenerateStream(const Key& key,
                                               const Nonce& nonce,
                                               size_t length,
                                               uint32_t counter) {
    std::vector<uint8_t> output;
    output.reserve(length);
    
    State state = InitState(key, nonce, counter);
    State blockOutput;
    
    while (output.size() < length) {
        // Generate block
        Block(blockOutput, state);
        
        // Convert state to bytes (little-endian)
        for (int i = 0; i < 16 && output.size() < length; i++) {
            if (output.size() < length) output.push_back((uint8_t)(blockOutput[i]));
            if (output.size() < length) output.push_back((uint8_t)(blockOutput[i] >> 8));
            if (output.size() < length) output.push_back((uint8_t)(blockOutput[i] >> 16));
            if (output.size() < length) output.push_back((uint8_t)(blockOutput[i] >> 24));
        }
        
        // Increment counter
        state[12]++;
    }
    
    // Secure cleanup
    SecureZeroMemory(&state, sizeof(state));
    SecureZeroMemory(&blockOutput, sizeof(blockOutput));
    
    return output;
}

std::vector<uint8_t> ChaCha20::Process(const Key& key,
                                        const Nonce& nonce,
                                        const std::vector<uint8_t>& data,
                                        uint32_t counter) {
    // Generate keystream and XOR with data
    std::vector<uint8_t> keystream = GenerateStream(key, nonce, data.size(), counter);
    
    std::vector<uint8_t> output(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        output[i] = data[i] ^ keystream[i];
    }
    
    // Secure cleanup
    SecureZeroMemory(keystream.data(), keystream.size());
    
    return output;
}

} // namespace Crypto
