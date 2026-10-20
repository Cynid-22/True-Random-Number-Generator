#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Crypto {

std::vector<uint8_t> GenerateQuadLayerStream(
    const uint8_t* entropyBytes,
    size_t entropyByteCount,
    const std::vector<uint8_t>& layerOneInfo,
    size_t byteCount);

std::vector<uint8_t> GenerateQuadLayerStream(
    const std::vector<uint8_t>& entropyBytes,
    const std::vector<uint8_t>& layerOneInfo,
    size_t byteCount);

}
