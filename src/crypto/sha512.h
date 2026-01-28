#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace Crypto {

// SHA-512 implementation per FIPS 180-4
// Produces 512-bit (64-byte) hash output
class SHA512 {
public:
    static constexpr size_t HASH_SIZE = 64;      // 512 bits = 64 bytes
    static constexpr size_t BLOCK_SIZE = 128;    // 1024 bits = 128 bytes
    
    using Hash = std::array<uint8_t, HASH_SIZE>;
    
    // Compute SHA-512 hash of data
    static Hash Compute(const uint8_t* data, size_t length);
    static Hash Compute(const std::vector<uint8_t>& data);
    
    // HMAC-SHA512 (keyed hash)
    static Hash HMAC(const uint8_t* key, size_t keyLen,
                     const uint8_t* data, size_t dataLen);
    static Hash HMAC(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& data);
    
private:
    // SHA-512 round constants (first 80 primes, cube roots)
    static const uint64_t K[80];
    
    // Initial hash values (first 8 primes, square roots)
    static const uint64_t H0[8];
    
    // Compression function
    static void Compress(uint64_t state[8], const uint8_t block[BLOCK_SIZE]);
    
    // Helper functions
    static inline uint64_t ROTR(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }
    static inline uint64_t Ch(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (~x & z); }
    static inline uint64_t Maj(uint64_t x, uint64_t y, uint64_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static inline uint64_t Sigma0(uint64_t x) { return ROTR(x, 28) ^ ROTR(x, 34) ^ ROTR(x, 39); }
    static inline uint64_t Sigma1(uint64_t x) { return ROTR(x, 14) ^ ROTR(x, 18) ^ ROTR(x, 41); }
    static inline uint64_t sigma0(uint64_t x) { return ROTR(x, 1) ^ ROTR(x, 8) ^ (x >> 7); }
    static inline uint64_t sigma1(uint64_t x) { return ROTR(x, 19) ^ ROTR(x, 61) ^ (x >> 6); }
};

} // namespace Crypto
