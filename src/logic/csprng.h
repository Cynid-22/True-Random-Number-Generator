#pragma once
#include <cstdint>
#include <cstddef>
#include <set>
#include <string>
#include <vector>
#include "../core/output_settings.h"
#include "../crypto/hmac_drbg.h"
#include "../entropy/pool.h"

namespace CSPRNG {

struct ReseedSource {
    const Entropy::EntropyPool& pool;
    const std::set<Entropy::EntropySource>& includedSources;
    Entropy::GenerationInput& input;
};

void SeedGenerator(Entropy::GenerationInput& input, uint64_t generationNumber);

std::string BuildGenerationInfo(const OutputSettings& settings, size_t byteCount,
                                uint64_t generationNumber);

std::vector<uint8_t> GenerateRandomBytes(Crypto::HmacDrbg& generator, size_t byteCount,
                                         Crypto::ByteSpan additionalInput = {},
                                         const ReseedSource* reseedSource = nullptr);

}
