#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace Crypto {

class SHA512 {
public:
    static constexpr size_t HASH_SIZE = 64;
    static constexpr size_t BLOCK_SIZE = 128;

    using Hash = std::array<uint8_t, HASH_SIZE>;

    static Hash Compute(const uint8_t* data, size_t length);
    static Hash Compute(const std::vector<uint8_t>& data);

    static Hash HMAC(const uint8_t* key, size_t keyLength,
                     const uint8_t* data, size_t dataLength);
    static Hash HMAC(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& data);

private:
    static const uint64_t ROUND_CONSTANTS[80];
    static const uint64_t INITIAL_STATE[8];

    static void Compress(uint64_t state[8], const uint8_t block[BLOCK_SIZE]);

    static inline uint64_t RotateRight(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
    static inline uint64_t Choose(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (~x & z); }
    static inline uint64_t Majority(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint64_t UpperSigma0(uint64_t x) { return RotateRight(x, 28) ^ RotateRight(x, 34) ^ RotateRight(x, 39); }
    static inline uint64_t UpperSigma1(uint64_t x) { return RotateRight(x, 14) ^ RotateRight(x, 18) ^ RotateRight(x, 41); }
    static inline uint64_t LowerSigma0(uint64_t x) { return RotateRight(x, 1) ^ RotateRight(x, 8) ^ (x >> 7); }
    static inline uint64_t LowerSigma1(uint64_t x) { return RotateRight(x, 19) ^ RotateRight(x, 61) ^ (x >> 6); }
};

}
