#pragma once
#include <cstdint>
#include <vector>

namespace CSPRNG {

std::vector<char> GenerateDecimal(const std::vector<uint8_t> &randomBytes,
                                  int digitCount,
                                  std::vector<uint8_t> &binaryValue);

std::vector<char> GenerateInteger(const std::vector<uint8_t> &randomBytes,
                                  int64_t min, int64_t max,
                                  bool &allDrawsRejected,
                                  std::vector<uint8_t> &binaryValue);

}
