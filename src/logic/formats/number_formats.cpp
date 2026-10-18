#include "number_formats.h"
#include "cancellation.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <windows.h>

namespace CSPRNG {

std::vector<char> GenerateDecimal(const std::vector<uint8_t> &randomBytes,
                                  int digitCount) {
  if (randomBytes.empty() || digitCount <= 0) {
    std::string text = "0.0";
    return std::vector<char>(text.begin(), text.end());
  }

  std::ostringstream stream;
  stream << "0.";

  const uint16_t ACCEPT_BELOW = 65530;

  size_t byteIndex = 0;

  for (int i = 0; i < digitCount; i++) {
    if ((i & 0x3FFF) == 0)
      ThrowIfGenerationCancelled();
    uint16_t draw = 0;
    bool accepted = false;

    while (byteIndex + 1 < randomBytes.size()) {
      draw = (static_cast<uint16_t>(randomBytes[byteIndex]) << 8) |
             randomBytes[byteIndex + 1];
      byteIndex += 2;

      if (draw < ACCEPT_BELOW) {
        accepted = true;
        break;
      }
    }

    if (!accepted) {
      draw = 0;
    }

    int digit = draw % 10;
    stream << digit;
  }

  std::string text = stream.str();
  std::vector<char> result(text.begin(), text.end());
  SecureZeroMemory(text.data(), text.size());
  return result;
}

std::vector<char> GenerateInteger(const std::vector<uint8_t> &randomBytes,
                                  int64_t min, int64_t max,
                                  bool &allDrawsRejected) {
  allDrawsRejected = false;
  if (min > max)
    std::swap(min, max);

  uint64_t range = static_cast<uint64_t>(max) - static_cast<uint64_t>(min) + 1;
  bool fullRange = (range == 0);

  uint64_t rejectBelow = fullRange ? 0 : (0 - range) % range;

  uint64_t draw = 0;
  bool accepted = false;
  size_t byteIndex = 0;

  while (byteIndex + 8 <= randomBytes.size()) {
    draw = 0;
    for (size_t i = 0; i < 8; i++) {
      draw = (draw << 8) | randomBytes[byteIndex++];
    }

    if (draw >= rejectBelow) {
      accepted = true;
      break;
    }
  }

  if (!accepted) {
    allDrawsRejected = true;
    return {};
  }

  uint64_t offset = fullRange ? draw : draw % range;
  int64_t value = static_cast<int64_t>(static_cast<uint64_t>(min) + offset);

  std::string text = std::to_string(value);
  std::vector<char> result(text.begin(), text.end());
  SecureZeroMemory(text.data(), text.size());
  return result;
}

}
