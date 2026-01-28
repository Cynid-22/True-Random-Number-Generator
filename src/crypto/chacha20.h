#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>

namespace Crypto {

// ChaCha20 stream cipher per RFC 8439
// Used for expanding entropy into arbitrary-length random streams
class ChaCha20 {
public:
    static constexpr size_t KEY_SIZE = 32;    // 256 bits
    static constexpr size_t NONCE_SIZE = 12;  // 96 bits (RFC 8439)
    static constexpr size_t BLOCK_SIZE = 64;  // 512 bits
    
    using Key = std::array<uint8_t, KEY_SIZE>;
    using Nonce = std::array<uint8_t, NONCE_SIZE>;
    
    // Generate a stream of random bytes
    // key: 256-bit key
    // nonce: 96-bit nonce (can be all zeros for single-use)
    // length: desired output length
    // counter: starting block counter (default 0)
    static std::vector<uint8_t> GenerateStream(
        const Key& key,
        const Nonce& nonce,
        size_t length,
        uint32_t counter = 0);
    
    // XOR data with ChaCha20 stream (for encryption/decryption)
    static std::vector<uint8_t> Process(
        const Key& key,
        const Nonce& nonce,
        const std::vector<uint8_t>& data,
        uint32_t counter = 0);
    
private:
    // ChaCha20 state: 16 x 32-bit words
    using State = std::array<uint32_t, 16>;
    
    // Generate a single 64-byte block
    static void Block(State& output, const State& input);
    
    // Quarter round function
    static inline void QuarterRound(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t& d);
    
    // Left rotate
    static inline uint32_t ROTL(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
    
    // Initialize state from key, nonce, and counter
    static State InitState(const Key& key, const Nonce& nonce, uint32_t counter);
};

} // namespace Crypto
