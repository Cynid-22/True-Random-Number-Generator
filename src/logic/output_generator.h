#pragma once
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "../core/output_settings.h"
#include "../entropy/entropy_common.h"
#include "csprng.h"

namespace CSPRNG {

struct GenerationResult {
    bool success;
    bool cancelled;
    std::vector<char> output;
    std::vector<uint8_t> binaryOutput;
    std::string errorMessage;
    GenerationMode mode;

    int oneTimePadResultMode;
    std::vector<char> oneTimePadKey;
    std::vector<char> oneTimePadCiphertext;
    std::string oneTimePadFileName;
};

size_t RandomByteCount(const OutputSettings& settings);

GenerationResult GenerateOutput(const std::set<Entropy::EntropySource>& includedSources,
                                const OutputSettings& settings);

}
