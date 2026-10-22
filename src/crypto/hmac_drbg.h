#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include "sha512.h"

namespace Crypto {

struct ByteSpan {
    const uint8_t* data = nullptr;
    size_t length = 0;
};

class HmacDrbg {
public:
    static constexpr size_t MAX_BYTES_PER_REQUEST = 65536;
    static constexpr uint64_t MAX_REQUESTS_BETWEEN_RESEEDS = 1ULL << 48;

    HmacDrbg(ByteSpan entropyInput, ByteSpan nonce, ByteSpan personalization);
    ~HmacDrbg();

    void Reseed(ByteSpan entropyInput, ByteSpan additionalInput);

    [[nodiscard]] bool Generate(uint8_t* output, size_t byteCount,
                                ByteSpan additionalInput = {});

private:
    void Update(std::initializer_list<ByteSpan> providedData);
    void UpdateRound(uint8_t separator, std::initializer_list<ByteSpan> providedData);

    SHA512::HMACKey m_key;
    SHA512::Hash m_value;
    uint64_t m_reseedCounter = 1;
};

}
