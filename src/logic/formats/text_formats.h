#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace CSPRNG {

std::vector<char> GenerateRandomCharacters(
    const std::vector<uint8_t> &randomBytes,
    int length,
    bool includeNumbers,
    bool includeUppercase,
    bool includeLowercase,
    bool includeSpecial,
    std::vector<uint8_t> &binaryValue);

std::vector<char> GeneratePassphrase(
    const std::vector<uint8_t> &randomBytes,
    int wordCount,
    const std::string &separator,
    const std::vector<std::string> &wordList,
    std::vector<uint8_t> &binaryValue);

}
