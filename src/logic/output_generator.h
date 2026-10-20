#pragma once
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "../core/output_settings.h"
#include "../entropy/entropy_common.h"

namespace CSPRNG {

enum class GenerationMode {
    EntropyBacked,
    Expanded
};

struct GenerationResult {
    bool success;
    bool cancelled;
    std::vector<char> output;
    std::vector<uint8_t> binaryOutput;
    std::string errorMessage;
    GenerationMode mode;
    float entropyConsumed;
    size_t rawBytesGenerated;

    int oneTimePadResultMode;
    std::vector<char> oneTimePadKey;
    std::vector<char> oneTimePadCiphertext;
    std::string oneTimePadFileName;
};

GenerationResult GenerateOutput(GenerationMode mode,
                                const std::set<Entropy::EntropySource>& includedSources,
                                const OutputSettings& settings);

}
