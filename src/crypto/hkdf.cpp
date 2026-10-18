#include "hkdf.h"
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace Crypto {

SHA512::Hash HKDF::Extract(const std::vector<uint8_t>& salt,
                           const std::vector<uint8_t>& inputKeyMaterial) {
    if (salt.empty()) {
        std::vector<uint8_t> zeroSalt(SHA512::HASH_SIZE, 0);
        return SHA512::HMAC(zeroSalt, inputKeyMaterial);
    }
    return SHA512::HMAC(salt, inputKeyMaterial);
}

std::vector<uint8_t> HKDF::Expand(const SHA512::Hash& pseudorandomKey,
                                   const std::vector<uint8_t>& info,
                                   size_t length) {
    if (length > MAX_OUTPUT_LENGTH) {
        throw std::runtime_error("HKDF: Requested length exceeds maximum");
    }

    std::vector<uint8_t> output;
    output.reserve(length);

    size_t blockCount = (length + SHA512::HASH_SIZE - 1) / SHA512::HASH_SIZE;

    SHA512::Hash previousBlock = {};
    size_t previousBlockLength = 0;

    for (size_t blockIndex = 1; blockIndex <= blockCount; blockIndex++) {
        std::vector<uint8_t> hmacInput;
        hmacInput.reserve(previousBlockLength + info.size() + 1);

        if (previousBlockLength > 0) {
            hmacInput.insert(hmacInput.end(), previousBlock.begin(), previousBlock.begin() + previousBlockLength);
        }

        hmacInput.insert(hmacInput.end(), info.begin(), info.end());

        hmacInput.push_back(static_cast<uint8_t>(blockIndex));

        std::vector<uint8_t> keyBytes(pseudorandomKey.begin(), pseudorandomKey.end());
        SHA512::Hash currentBlock = SHA512::HMAC(keyBytes, hmacInput);

        size_t bytesToAppend = std::min(SHA512::HASH_SIZE, length - output.size());
        output.insert(output.end(), currentBlock.begin(), currentBlock.begin() + bytesToAppend);

        previousBlock = currentBlock;
        previousBlockLength = SHA512::HASH_SIZE;

        SecureZeroMemory(hmacInput.data(), hmacInput.size());
        SecureZeroMemory(keyBytes.data(), keyBytes.size());
    }

    SecureZeroMemory(previousBlock.data(), previousBlock.size());

    return output;
}

std::vector<uint8_t> HKDF::DeriveKey(const std::vector<uint8_t>& inputKeyMaterial,
                                      const std::vector<uint8_t>& salt,
                                      const std::vector<uint8_t>& info,
                                      size_t length) {
    SHA512::Hash pseudorandomKey = Extract(salt, inputKeyMaterial);
    std::vector<uint8_t> result = Expand(pseudorandomKey, info, length);

    SecureZeroMemory(pseudorandomKey.data(), pseudorandomKey.size());

    return result;
}

}
