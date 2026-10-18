#pragma once
#include <cstddef>
#include <string>
#include <vector>

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
};

GenerationResult GenerateOutput(GenerationMode mode);

}
