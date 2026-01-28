#include "sha512.h"
#include <cstring>
#include <windows.h> // For SecureZeroMemory

namespace Crypto {

// SHA-512 round constants K[0..79] - cube roots of first 80 primes
const uint64_t SHA512::K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

// Initial hash values H0[0..7] - square roots of first 8 primes
const uint64_t SHA512::H0[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

void SHA512::Compress(uint64_t state[8], const uint8_t block[BLOCK_SIZE]) {
    uint64_t W[80];
    
    // Prepare message schedule W[0..79]
    for (int t = 0; t < 16; t++) {
        W[t] = ((uint64_t)block[t * 8 + 0] << 56) |
               ((uint64_t)block[t * 8 + 1] << 48) |
               ((uint64_t)block[t * 8 + 2] << 40) |
               ((uint64_t)block[t * 8 + 3] << 32) |
               ((uint64_t)block[t * 8 + 4] << 24) |
               ((uint64_t)block[t * 8 + 5] << 16) |
               ((uint64_t)block[t * 8 + 6] << 8) |
               ((uint64_t)block[t * 8 + 7]);
    }
    
    for (int t = 16; t < 80; t++) {
        W[t] = sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t - 16];
    }
    
    // Initialize working variables
    uint64_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint64_t e = state[4], f = state[5], g = state[6], h = state[7];
    
    // 80 rounds
    for (int t = 0; t < 80; t++) {
        uint64_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[t] + W[t];
        uint64_t T2 = Sigma0(a) + Maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }
    
    // Add compressed chunk to current hash value
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    
    // Secure cleanup of working variables
    SecureZeroMemory(W, sizeof(W));
}

SHA512::Hash SHA512::Compute(const uint8_t* data, size_t length) {
    // Initialize state
    uint64_t state[8];
    memcpy(state, H0, sizeof(state));
    
    // Process complete blocks
    size_t numBlocks = length / BLOCK_SIZE;
    for (size_t i = 0; i < numBlocks; i++) {
        Compress(state, data + i * BLOCK_SIZE);
    }
    
    // Prepare final block(s) with padding
    size_t remaining = length % BLOCK_SIZE;
    uint8_t finalBlock[BLOCK_SIZE * 2]; // May need 2 blocks for padding
    memset(finalBlock, 0, sizeof(finalBlock));
    memcpy(finalBlock, data + numBlocks * BLOCK_SIZE, remaining);
    
    // Append bit '1' (0x80)
    finalBlock[remaining] = 0x80;
    
    // Determine if we need 1 or 2 final blocks
    size_t padBlocks = 1;
    if (remaining >= 112) { // Not enough room for length (128 - 16 = 112)
        padBlocks = 2;
    }
    
    // Append length in bits as 128-bit big-endian (we only use lower 64 bits)
    uint64_t bitLength = length * 8;
    size_t lenOffset = padBlocks * BLOCK_SIZE - 8;
    finalBlock[lenOffset + 0] = (uint8_t)(bitLength >> 56);
    finalBlock[lenOffset + 1] = (uint8_t)(bitLength >> 48);
    finalBlock[lenOffset + 2] = (uint8_t)(bitLength >> 40);
    finalBlock[lenOffset + 3] = (uint8_t)(bitLength >> 32);
    finalBlock[lenOffset + 4] = (uint8_t)(bitLength >> 24);
    finalBlock[lenOffset + 5] = (uint8_t)(bitLength >> 16);
    finalBlock[lenOffset + 6] = (uint8_t)(bitLength >> 8);
    finalBlock[lenOffset + 7] = (uint8_t)(bitLength);
    
    // Process final block(s)
    for (size_t i = 0; i < padBlocks; i++) {
        Compress(state, finalBlock + i * BLOCK_SIZE);
    }
    
    // Produce hash output (big-endian)
    Hash hash;
    for (int i = 0; i < 8; i++) {
        hash[i * 8 + 0] = (uint8_t)(state[i] >> 56);
        hash[i * 8 + 1] = (uint8_t)(state[i] >> 48);
        hash[i * 8 + 2] = (uint8_t)(state[i] >> 40);
        hash[i * 8 + 3] = (uint8_t)(state[i] >> 32);
        hash[i * 8 + 4] = (uint8_t)(state[i] >> 24);
        hash[i * 8 + 5] = (uint8_t)(state[i] >> 16);
        hash[i * 8 + 6] = (uint8_t)(state[i] >> 8);
        hash[i * 8 + 7] = (uint8_t)(state[i]);
    }
    
    // Secure cleanup
    SecureZeroMemory(state, sizeof(state));
    SecureZeroMemory(finalBlock, sizeof(finalBlock));
    
    return hash;
}

SHA512::Hash SHA512::Compute(const std::vector<uint8_t>& data) {
    return Compute(data.data(), data.size());
}

SHA512::Hash SHA512::HMAC(const uint8_t* key, size_t keyLen,
                          const uint8_t* data, size_t dataLen) {
    uint8_t keyBlock[BLOCK_SIZE];
    memset(keyBlock, 0, sizeof(keyBlock));
    
    // If key > block size, hash it first
    if (keyLen > BLOCK_SIZE) {
        Hash keyHash = Compute(key, keyLen);
        memcpy(keyBlock, keyHash.data(), HASH_SIZE);
    } else {
        memcpy(keyBlock, key, keyLen);
    }
    
    // Create inner and outer padded keys
    uint8_t ipad[BLOCK_SIZE];
    uint8_t opad[BLOCK_SIZE];
    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        ipad[i] = keyBlock[i] ^ 0x36;
        opad[i] = keyBlock[i] ^ 0x5c;
    }
    
    // Inner hash: H(ipad || data)
    std::vector<uint8_t> innerData(BLOCK_SIZE + dataLen);
    memcpy(innerData.data(), ipad, BLOCK_SIZE);
    memcpy(innerData.data() + BLOCK_SIZE, data, dataLen);
    Hash innerHash = Compute(innerData.data(), innerData.size());
    
    // Outer hash: H(opad || innerHash)
    std::vector<uint8_t> outerData(BLOCK_SIZE + HASH_SIZE);
    memcpy(outerData.data(), opad, BLOCK_SIZE);
    memcpy(outerData.data() + BLOCK_SIZE, innerHash.data(), HASH_SIZE);
    Hash result = Compute(outerData.data(), outerData.size());
    
    // Secure cleanup
    SecureZeroMemory(keyBlock, sizeof(keyBlock));
    SecureZeroMemory(ipad, sizeof(ipad));
    SecureZeroMemory(opad, sizeof(opad));
    SecureZeroMemory(innerData.data(), innerData.size());
    SecureZeroMemory(outerData.data(), outerData.size());
    SecureZeroMemory(innerHash.data(), innerHash.size());
    
    return result;
}

SHA512::Hash SHA512::HMAC(const std::vector<uint8_t>& key,
                          const std::vector<uint8_t>& data) {
    return HMAC(key.data(), key.size(), data.data(), data.size());
}

} // namespace Crypto
