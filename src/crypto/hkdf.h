#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "sha512.h"

namespace Crypto {

class HKDF {
public:
    static constexpr size_t MAX_OUTPUT_LENGTH = 255 * SHA512::HASH_SIZE;

    static std::vector<uint8_t> DeriveKey(
        const std::vector<uint8_t>& inputKeyMaterial,
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& info,
        size_t length);

    static SHA512::Hash Extract(
        const std::vector<uint8_t>& salt,
        const std::vector<uint8_t>& inputKeyMaterial);

    static std::vector<uint8_t> Expand(
        const SHA512::Hash& pseudorandomKey,
        const std::vector<uint8_t>& info,
        size_t length);
};

}
