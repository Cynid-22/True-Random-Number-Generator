#include "text_formats.h"
#include "cancellation.h"
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
  uint16_t acceptBelow = 65535 - (65535 % static_cast<uint16_t>(characterSetSize));

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

    if (!accepted)
      draw = 0;

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

  std::string result;
  size_t bytesPerWord = 3;

  for (int i = 0; i < wordCount; i++) {
    if ((i & 0x3FF) == 0)
      ThrowIfGenerationCancelled();
    size_t offset = i * bytesPerWord;
    uint32_t randomIndex = 0;

    for (size_t j = 0; j < bytesPerWord && (offset + j) < randomBytes.size();
         j++) {
      randomIndex = (randomIndex << 8) | randomBytes[offset + j];
    }

    size_t wordIndex = randomIndex % wordList.size();

    if (i > 0)
      result += separator;
    result += wordList[wordIndex];
  }

  std::vector<char> passphrase(result.begin(), result.end());
  SecureZeroMemory(result.data(), result.size());
  return passphrase;
}

}
