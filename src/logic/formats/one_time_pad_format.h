#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace CSPRNG {

std::vector<char> GenerateOneTimePadText(
    const std::vector<uint8_t> &randomBytes,
    const std::string &message);

std::vector<uint8_t> GenerateOneTimePadFile(
    const std::vector<uint8_t> &randomBytes,
    const std::vector<uint8_t> &fileData);

}
