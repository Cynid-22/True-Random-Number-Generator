#pragma once
#include <cstddef>
#include <set>
#include <string>
#include <vector>
#include "../entropy/entropy_common.h"

namespace CSPRNG {

enum class GenerationMode {
    Consolidation,
    Expansion
};

struct GenerationResult {
    bool success;
    bool cancelled;
    std::vector<char> output;
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
                                const std::set<Entropy::EntropySource>& includedSources);

}
