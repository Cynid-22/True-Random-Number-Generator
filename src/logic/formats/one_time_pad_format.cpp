#include "one_time_pad_format.h"
#include "cancellation.h"
#include <windows.h>

namespace CSPRNG {

static int OneTimePadAlphabetIndex(char character) {
  if (character >= '0' && character <= '9')
    return character - '0';
  if (character >= 'A' && character <= 'Z')
    return 10 + (character - 'A');
  if (character >= 'a' && character <= 'z')
    return 36 + (character - 'a');
  if (character == ' ')
    return 62;
  return -1;
}

static bool DrawUniformAlphabetIndex(const std::vector<uint8_t> &randomBytes,
                                     size_t &randomByteIndex,
                                     int &alphabetIndex) {
  constexpr int acceptBelow = 256 - (256 % ONE_TIME_PAD_ALPHABET_SIZE);
  while (randomByteIndex < randomBytes.size()) {
    uint8_t draw = randomBytes[randomByteIndex++];
    if (draw < acceptBelow) {
      alphabetIndex = draw % ONE_TIME_PAD_ALPHABET_SIZE;
      return true;
    }
  }
  return false;
}

static std::vector<char> OneTimePadError(const char *message) {
  std::string error = message;
  return std::vector<char>(error.begin(), error.end());
}

std::vector<char> GenerateOneTimePadText(const std::vector<uint8_t> &randomBytes,
                                         const std::string &message) {

  std::string result;
  result.reserve(message.size());
  size_t randomByteIndex = 0;

  for (char character : message) {
    int messageValue = OneTimePadAlphabetIndex(character);
    if (messageValue < 0) {
      SecureZeroMemory(result.data(), result.size());
      return OneTimePadError("[Error: Message may only contain letters (A-Z, "
                             "a-z), numbers (0-9) and spaces. No "
                             "punctuation.]");
    }

    int keyValue = 0;
    if (!DrawUniformAlphabetIndex(randomBytes, randomByteIndex, keyValue)) {
      SecureZeroMemory(result.data(), result.size());
      return OneTimePadError("[Error: Insufficient entropy (rejection sampling "
                             "exhausted). Please retry.]");
    }

    int cipherValue = (messageValue + keyValue) % ONE_TIME_PAD_ALPHABET_SIZE;
    result += ONE_TIME_PAD_ALPHABET[cipherValue];
  }

  std::vector<char> ciphertext(result.begin(), result.end());
  SecureZeroMemory(result.data(), result.size());
  return ciphertext;
}

std::vector<char> GenerateOneTimePadKey(const std::vector<uint8_t> &randomBytes,
                                        int length) {
  if (length <= 0)
    return {};

  std::vector<char> key;
  key.reserve(static_cast<size_t>(length) + 1);
  size_t randomByteIndex = 0;

  for (int i = 0; i < length; i++) {
    if ((i & 0x3FFF) == 0)
      ThrowIfGenerationCancelled();

    int keyValue = 0;
    if (!DrawUniformAlphabetIndex(randomBytes, randomByteIndex, keyValue)) {
      SecureZeroMemory(key.data(), key.size());
      return OneTimePadError("[Error: Insufficient entropy (rejection sampling "
                             "exhausted). Please retry.]");
    }
    key.push_back(ONE_TIME_PAD_ALPHABET[keyValue]);
  }

  return key;
}

std::vector<uint8_t> GenerateOneTimePadFile(const std::vector<uint8_t> &randomBytes,
                                            const std::vector<uint8_t> &fileData) {

  std::vector<uint8_t> result(fileData.size());

  size_t keySize = randomBytes.size();
  for (size_t i = 0; i < fileData.size(); i++) {
    result[i] = fileData[i] ^ randomBytes[i % keySize];
  }

  return result;
}

}
