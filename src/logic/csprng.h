#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "../core/output_settings.h"
#include "../crypto/secure_mem.h"
#include "../entropy/pool.h"

namespace CSPRNG {

Crypto::SecureBytes BuildSeedMaterial(const Entropy::GenerationInput& input);

Entropy::CarryOver DeriveCarryOver(const Crypto::SecureBytes& seedMaterial);

std::vector<uint8_t> GenerateRandomBytes(
    const OutputSettings& settings,
    const Crypto::SecureBytes& seedMaterial,
    uint64_t generationNumber,
    size_t byteCount,
    uint32_t attempt = 0);

}
