#include "text_formats.h"
#include "cancellation.h"
#include <stdexcept>
#include <windows.h>

namespace CSPRNG {

std::vector<char> GenerateRandomCharacters(const std::vector<uint8_t> &randomBytes,
                                           int length, bool includeNumbers,
                                           bool includeUppercase,
                                           bool includeLowercase,
                                           bool includeSpecial) {
  std::string characterSet;
  if (includeNumbers)
    characterSet += "0123456789";
  if (includeUppercase)
    characterSet += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (includeLowercase)
    characterSet += "abcdefghijklmnopqrstuvwxyz";
  if (includeSpecial)
    characterSet += "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  if (characterSet.empty() || randomBytes.empty() || length <= 0)
    return {};

  std::vector<char> result;
  result.reserve(length);

  size_t characterSetSize = characterSet.size();
  const uint32_t acceptBelow =
      65536u - (65536u % static_cast<uint32_t>(characterSetSize));

  size_t byteIndex = 0;

  for (int i = 0; i < length; i++) {
    if ((i & 0x3FFF) == 0)
      ThrowIfGenerationCancelled();
    uint16_t draw = 0;
    bool accepted = false;

    while (byteIndex + 1 < randomBytes.size()) {
      draw = (static_cast<uint16_t>(randomBytes[byteIndex]) << 8) |
             randomBytes[byteIndex + 1];
      byteIndex += 2;

      if (draw < acceptBelow) {
        accepted = true;
        break;
      }
    }

    if (!accepted) {
      SecureZeroMemory(result.data(), result.size());
      throw std::runtime_error(
          "Random Characters: too many random draws were rejected. Please "
          "try again.");
    }

    size_t characterIndex = draw % characterSetSize;
    result.push_back(characterSet[characterIndex]);
  }

  return result;
}

std::vector<char> GeneratePassphrase(const std::vector<uint8_t> &randomBytes,
                                     int wordCount,
                                     const std::string &separator,
                                     const std::vector<std::string> &wordList) {

  std::string error = "[Error: Wordlist not loaded]";
  if (wordList.empty() || randomBytes.empty() || wordCount <= 0) {
    return std::vector<char>(error.begin(), error.end());
  }

  const uint32_t THREE_BYTE_DRAW_RANGE = 1u << 24;
  if (wordList.size() > THREE_BYTE_DRAW_RANGE) {
    std::string tooLargeError = "[Error: Wordlist is too large]";
    return std::vector<char>(tooLargeError.begin(), tooLargeError.end());
  }
  const uint32_t wordListSize = static_cast<uint32_t>(wordList.size());
  const uint32_t acceptBelow =
      THREE_BYTE_DRAW_RANGE - (THREE_BYTE_DRAW_RANGE % wordListSize);

  std::string result;
  size_t byteIndex = 0;

  try {
    for (int i = 0; i < wordCount; i++) {
      if ((i & 0x3FF) == 0)
        ThrowIfGenerationCancelled();

      uint32_t draw = 0;
      do {
        if (byteIndex + 3 > randomBytes.size())
          throw std::runtime_error(
              "Passphrase: too many random draws were rejected. Please try "
              "again.");
        draw = (static_cast<uint32_t>(randomBytes[byteIndex]) << 16) |
               (static_cast<uint32_t>(randomBytes[byteIndex + 1]) << 8) |
               randomBytes[byteIndex + 2];
        byteIndex += 3;
      } while (draw >= acceptBelow);

      if (i > 0)
        result += separator;
      result += wordList[draw % wordListSize];
    }
  } catch (...) {
    SecureZeroMemory(result.data(), result.size());
    throw;
  }

  std::vector<char> passphrase(result.begin(), result.end());
  SecureZeroMemory(result.data(), result.size());
  return passphrase;
}

}
