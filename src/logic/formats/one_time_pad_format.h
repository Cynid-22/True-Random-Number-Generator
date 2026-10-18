#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace CSPRNG {

inline constexpr char ONE_TIME_PAD_ALPHABET[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ";
inline constexpr int ONE_TIME_PAD_ALPHABET_SIZE = sizeof(ONE_TIME_PAD_ALPHABET) - 1;

inline bool IsOneTimePadCharacter(unsigned int character) {
  return (character >= '0' && character <= '9') ||
         (character >= 'A' && character <= 'Z') ||
         (character >= 'a' && character <= 'z') || character == ' ';
}

std::vector<char> GenerateOneTimePadText(
    const std::vector<uint8_t> &randomBytes,
    const std::string &message);

std::vector<char> GenerateOneTimePadKey(
    const std::vector<uint8_t> &randomBytes,
    int length);

std::vector<uint8_t> GenerateOneTimePadFile(
    const std::vector<uint8_t> &randomBytes,
    const std::vector<uint8_t> &fileData);

}
