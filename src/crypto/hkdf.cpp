#include "hkdf.h"
#include <cstring>
#include <stdexcept>
#include <windows.h> // For SecureZeroMemory

namespace Crypto {

SHA512::Hash HKDF::Extract(const std::vector<uint8_t>& salt,
                           const std::vector<uint8_t>& ikm) {
    // If salt is empty, use a string of HashLen zeros
    if (salt.empty()) {
        std::vector<uint8_t> defaultSalt(SHA512::HASH_SIZE, 0);
        return SHA512::HMAC(defaultSalt, ikm);
    }
    return SHA512::HMAC(salt, ikm);
}

std::vector<uint8_t> HKDF::Expand(const SHA512::Hash& prk,
                                   const std::vector<uint8_t>& info,
                                   size_t length) {
    if (length > MAX_OUTPUT_LENGTH) {
        throw std::runtime_error("HKDF: Requested length exceeds maximum");
    }
    
    std::vector<uint8_t> output;
    output.reserve(length);
    
    // Number of blocks needed: ceil(length / HashLen)
    size_t n = (length + SHA512::HASH_SIZE - 1) / SHA512::HASH_SIZE;
    
    SHA512::Hash T_prev = {}; // T(0) = empty string
    size_t T_prev_len = 0;
    
    for (size_t i = 1; i <= n; i++) {
        // T(i) = HMAC(PRK, T(i-1) || info || i)
        std::vector<uint8_t> data;
        data.reserve(T_prev_len + info.size() + 1);
        
        // Append T(i-1)
        if (T_prev_len > 0) {
            data.insert(data.end(), T_prev.begin(), T_prev.begin() + T_prev_len);
        }
        
        // Append info
        data.insert(data.end(), info.begin(), info.end());
        
        // Append counter byte
        data.push_back(static_cast<uint8_t>(i));
        
        // Compute HMAC
        std::vector<uint8_t> prkVec(prk.begin(), prk.end());
        SHA512::Hash T_i = SHA512::HMAC(prkVec, data);
        
        // Append to output (may be partial for last block)
        size_t toAppend = std::min(SHA512::HASH_SIZE, length - output.size());
        output.insert(output.end(), T_i.begin(), T_i.begin() + toAppend);
        
        // Save for next iteration
        T_prev = T_i;
        T_prev_len = SHA512::HASH_SIZE;
        
        // Secure cleanup
        SecureZeroMemory(data.data(), data.size());
        SecureZeroMemory(prkVec.data(), prkVec.size());
    }
    
    // Secure cleanup
    SecureZeroMemory(T_prev.data(), T_prev.size());
    
    return output;
}

std::vector<uint8_t> HKDF::DeriveKey(const std::vector<uint8_t>& ikm,
                                      const std::vector<uint8_t>& salt,
                                      const std::vector<uint8_t>& info,
                                      size_t length) {
    SHA512::Hash prk = Extract(salt, ikm);
    std::vector<uint8_t> result = Expand(prk, info, length);
    
    // Secure cleanup
    SecureZeroMemory(prk.data(), prk.size());
    
    return result;
}

} // namespace Crypto
