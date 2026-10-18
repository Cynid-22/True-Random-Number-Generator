#include "one_time_pad_format.h"
#include <windows.h>

namespace CSPRNG {

std::vector<char> GenerateOneTimePadText(const std::vector<uint8_t> &randomBytes,
                                         const std::string &message) {

  std::string result;
  result.reserve(message.size());
  size_t keyIndex = 0;

  for (char character : message) {
    if (character < 32 || character > 126) {
      std::string error = "[Error: Message contains non-ASCII characters. Only "
                          "printable ASCII (32-126) allowed.]";
      return std::vector<char>(error.begin(), error.end());
    }

    uint8_t keyValue = 0;
    bool accepted = false;

    while (keyIndex < randomBytes.size()) {
      uint8_t draw = randomBytes[keyIndex++];
      if (draw < 190) {
        keyValue = draw % 95;
        accepted = true;
        break;
      }
    }

    if (!accepted) {
      std::string error = "[Error: Insufficient entropy (rejection sampling "
                          "exhausted). Please retry.]";
      return std::vector<char>(error.begin(), error.end());
    }

    int messageValue = static_cast<int>(character) - 32;
    int cipherValue = (messageValue + static_cast<int>(keyValue)) % 95;
    result += static_cast<char>(cipherValue + 32);
  }

  std::vector<char> ciphertext(result.begin(), result.end());
  SecureZeroMemory(result.data(), result.size());
  return ciphertext;
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
