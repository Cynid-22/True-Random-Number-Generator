#pragma once
#include <cstddef>
#include <cstdint>

namespace Crypto {

class ToeplitzExtractor {
public:
    static constexpr size_t OUTPUT_BITS = 512;
    static constexpr size_t OUTPUT_BYTES = OUTPUT_BITS / 8;
    static constexpr size_t MAX_INPUT_BITS = 131072;
    static constexpr size_t MAX_INPUT_BYTES = MAX_INPUT_BITS / 8;
    static constexpr size_t SEED_BITS = MAX_INPUT_BITS + OUTPUT_BITS - 1;

    [[nodiscard]] static bool Extract(const uint8_t* input, size_t inputLength,
                                      uint8_t output[OUTPUT_BYTES]);
};

}
