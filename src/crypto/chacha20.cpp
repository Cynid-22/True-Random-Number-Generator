#include "chacha20.h"
#include <cstring>
#include <windows.h>

namespace Crypto {

static const uint32_t EXPAND_32_BYTE_K[4] = {
    0x61707865,
    0x3320646e,
    0x79622d32,
    0x6b206574
};

inline void ChaCha20::QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d) {
    a += b; d ^= a; d = RotateLeft(d, 16);
    c += d; b ^= c; b = RotateLeft(b, 12);
    a += b; d ^= a; d = RotateLeft(d, 8);
    c += d; b ^= c; b = RotateLeft(b, 7);
}

ChaCha20::State ChaCha20::CreateInitialState(const Key& key, const Nonce& nonce, uint32_t counter) {
    State state;

    state[0] = EXPAND_32_BYTE_K[0];
    state[1] = EXPAND_32_BYTE_K[1];
    state[2] = EXPAND_32_BYTE_K[2];
    state[3] = EXPAND_32_BYTE_K[3];

    for (int i = 0; i < 8; i++) {
        state[4 + i] = ((uint32_t)key[i * 4 + 0]) |
                       ((uint32_t)key[i * 4 + 1] << 8) |
                       ((uint32_t)key[i * 4 + 2] << 16) |
                       ((uint32_t)key[i * 4 + 3] << 24);
    }

    state[12] = counter;

    for (int i = 0; i < 3; i++) {
        state[13 + i] = ((uint32_t)nonce[i * 4 + 0]) |
                        ((uint32_t)nonce[i * 4 + 1] << 8) |
                        ((uint32_t)nonce[i * 4 + 2] << 16) |
                        ((uint32_t)nonce[i * 4 + 3] << 24);
    }

    return state;
}

void ChaCha20::ComputeBlock(State& output, const State& input) {
    State working = input;

    for (int i = 0; i < 10; i++) {
        QuarterRound(working[0], working[4], working[8],  working[12]);
        QuarterRound(working[1], working[5], working[9],  working[13]);
        QuarterRound(working[2], working[6], working[10], working[14]);
        QuarterRound(working[3], working[7], working[11], working[15]);

        QuarterRound(working[0], working[5], working[10], working[15]);
        QuarterRound(working[1], working[6], working[11], working[12]);
        QuarterRound(working[2], working[7], working[8],  working[13]);
        QuarterRound(working[3], working[4], working[9],  working[14]);
    }

    for (int i = 0; i < 16; i++) {
        output[i] = working[i] + input[i];
    }

    SecureZeroMemory(&working, sizeof(working));
}

std::vector<uint8_t> ChaCha20::GenerateStream(const Key& key,
                                               const Nonce& nonce,
                                               size_t length,
                                               uint32_t counter) {
    std::vector<uint8_t> output;
    output.reserve(length);

    State state = CreateInitialState(key, nonce, counter);
    State keystreamBlock;

    while (output.size() < length) {
        ComputeBlock(keystreamBlock, state);

        for (int i = 0; i < 16 && output.size() < length; i++) {
            if (output.size() < length) output.push_back((uint8_t)(keystreamBlock[i]));
            if (output.size() < length) output.push_back((uint8_t)(keystreamBlock[i] >> 8));
            if (output.size() < length) output.push_back((uint8_t)(keystreamBlock[i] >> 16));
            if (output.size() < length) output.push_back((uint8_t)(keystreamBlock[i] >> 24));
        }

        state[12]++;
    }

    SecureZeroMemory(&state, sizeof(state));
    SecureZeroMemory(&keystreamBlock, sizeof(keystreamBlock));

    return output;
}

}
