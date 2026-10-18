#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace Crypto {

class ChaCha20 {
public:
    static constexpr size_t KEY_SIZE = 32;
    static constexpr size_t NONCE_SIZE = 12;
    static constexpr size_t BLOCK_SIZE = 64;

    using Key = std::array<uint8_t, KEY_SIZE>;
    using Nonce = std::array<uint8_t, NONCE_SIZE>;

    static std::vector<uint8_t> GenerateStream(
        const Key& key,
        const Nonce& nonce,
        size_t length,
        uint32_t counter = 0);

private:
    using State = std::array<uint32_t, 16>;

    static void ComputeBlock(State& output, const State& input);

    static inline void QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d);

    static inline uint32_t RotateLeft(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    static State CreateInitialState(const Key& key, const Nonce& nonce, uint32_t counter);
};

}
