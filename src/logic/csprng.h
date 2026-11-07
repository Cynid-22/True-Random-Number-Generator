#pragma once
#include <cstdint>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "../core/output_settings.h"
#include "../crypto/hmac_drbg.h"
#include "../entropy/pool.h"

namespace CSPRNG {

enum class GenerationMode {
    EntropyBacked,
    Expanded
};

// SP 800-90C 6.4.2: the DRBG of an RBG3(XOR) is reseeded after 2^17 bits of output
constexpr uint64_t TRUE_PATH_RESEED_INTERVAL_BYTES = 16384;
constexpr size_t TRUE_PATH_RESEED_BYTES = 32;
constexpr size_t GENERATOR_RESERVE_SEED_BYTES =
    static_cast<size_t>(AppConfig::MINIMUM_ENTROPY_BITS) / 8;

class ReserveRanOut : public std::runtime_error {
public:
    ReserveRanOut() : std::runtime_error("The true random reserve ran out during the output.") {}
};

struct ReseedSource {
    const Entropy::EntropyPool& pool;
    const std::set<Entropy::EntropySource>& includedSources;
};

bool GeneratorNeedsReserveSeed(const Entropy::GenerationInput& input);

void SeedGenerator(Entropy::GenerationInput& input, uint64_t generationNumber);

std::string BuildGenerationInfo(const OutputSettings& settings, size_t byteCount,
                                uint64_t generationNumber);

size_t TruePathReserveBytes(size_t outputBytes, uint64_t generatorBytesSinceFullReseed);

std::vector<uint8_t> GenerateRandomBytes(Entropy::GenerationInput& input, size_t byteCount,
                                         GenerationMode mode,
                                         Crypto::ByteSpan additionalInput = {},
                                         const ReseedSource* reseedSource = nullptr);

}
