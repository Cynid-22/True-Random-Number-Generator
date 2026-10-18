#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "../entropy/entropy_common.h"

namespace CSPRNG {

std::vector<uint8_t> GenerateRandomBytes(
    const std::vector<Entropy::EntropyDataPoint>& entropyData,
    size_t byteCount,
    uint32_t attempt = 0);

}
