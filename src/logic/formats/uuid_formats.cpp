#include "uuid_formats.h"
#include "../../../config/AppConfig.h"
#include "../../core/output_format.h"
#include <algorithm>
#include <chrono>
#include <windows.h>

namespace CSPRNG {

static std::vector<char> FormatUuidText(const uint8_t uuidBytes[16],
                                        bool uppercase, bool hyphens) {
  const char *hexDigits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  std::vector<char> result;
  result.reserve(36);
  for (int i = 0; i < 16; i++) {
    if (hyphens && (i == 4 || i == 6 || i == 8 || i == 10))
      result.push_back('-');
    result.push_back(hexDigits[uuidBytes[i] >> 4]);
    result.push_back(hexDigits[uuidBytes[i] & 0x0F]);
  }
  return result;
}

std::vector<char> GenerateUUIDv4(const std::vector<uint8_t> &randomBytes,
                                 bool uppercase, bool hyphens,
                                 std::vector<uint8_t> &binaryValue) {
  binaryValue.clear();
  if (randomBytes.size() < 16)
    return {};

  uint8_t uuidBytes[16];
  std::copy(randomBytes.begin(), randomBytes.begin() + 16, uuidBytes);

  uuidBytes[6] = static_cast<uint8_t>((uuidBytes[6] & 0x0F) | 0x40);
  uuidBytes[8] = static_cast<uint8_t>((uuidBytes[8] & 0x3F) | 0x80);

  std::vector<char> result = FormatUuidText(uuidBytes, uppercase, hyphens);
  binaryValue.assign(uuidBytes, uuidBytes + sizeof(uuidBytes));
  SecureZeroMemory(uuidBytes, sizeof(uuidBytes));
  return result;
}

std::vector<char> GenerateUUIDv7(const std::vector<uint8_t> &randomBytes,
                                 bool uppercase, bool hyphens,
                                 const UUIDv7Options &options,
                                 UUIDv7State &state,
                                 std::vector<uint8_t> &binaryValue) {
  binaryValue.clear();
  if (randomBytes.size() < 16)
    return {};

  const uint64_t RAND_A_MASK = 0xFFF;
  const uint64_t RAND_B_MASK = (1ULL << 62) - 1;

  const uint8_t *bytes = randomBytes.data();
  uint64_t freshRandB = 0;
  for (int i = 0; i < 8; i++)
    freshRandB = (freshRandB << 8) | bytes[i];
  freshRandB &= RAND_B_MASK;
  uint64_t freshRandA = ((static_cast<uint64_t>(bytes[8]) << 8) | bytes[9]) & RAND_A_MASK;
  uint64_t seedOrIncrementBits = 0;
  for (int i = 10; i < 16; i++)
    seedOrIncrementBits = (seedOrIncrementBits << 8) | bytes[i];

  uint64_t nowMs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());

  bool isNewMillisecond = !state.hasLastTimestamp || nowMs > state.lastTimestampMs;
  if (isNewMillisecond) {
    state.lastTimestampMs = nowMs;
    state.hasLastTimestamp = true;
  }

  uint64_t randA = freshRandA;
  uint64_t randB = freshRandB;

  switch (options.ordering) {
  case UUIDV7_DEDICATED_COUNTER: {
    int counterBits = std::min(std::max(options.counterBits,
                                        AppConfig::UUIDV7_MIN_COUNTER_BITS),
                               AppConfig::UUIDV7_MAX_COUNTER_BITS);
    uint64_t counterMax = (1ULL << counterBits) - 1;
    uint64_t counterSeed = seedOrIncrementBits & (counterMax >> 1);

    if (isNewMillisecond) {
      state.counter = counterSeed;
    } else if (++state.counter > counterMax) {
      state.lastTimestampMs++;
      state.counter = counterSeed;
    }

    if (counterBits <= 12) {
      int randomBitsInRandA = 12 - counterBits;
      randA = (state.counter << randomBitsInRandA) |
              (freshRandA & ((1ULL << randomBitsInRandA) - 1));
    } else {
      int counterBitsInRandB = counterBits - 12;
      int randomBitsInRandB = 74 - counterBits;
      randA = state.counter >> counterBitsInRandB;
      randB = ((state.counter & ((1ULL << counterBitsInRandB) - 1)) << randomBitsInRandB) |
              (freshRandB & ((1ULL << randomBitsInRandB) - 1));
    }
    break;
  }

  case UUIDV7_MONOTONIC_RANDOM: {
    int incrementBits = std::min(std::max(options.incrementBits,
                                          AppConfig::UUIDV7_MIN_INCREMENT_BITS),
                                 AppConfig::UUIDV7_MAX_INCREMENT_BITS);
    uint64_t increment = (seedOrIncrementBits & ((1ULL << incrementBits) - 1)) + 1;

    bool reseed = isNewMillisecond;
    if (!isNewMillisecond) {
      state.monotonicRandB += increment;
      if (state.monotonicRandB > RAND_B_MASK) {
        state.monotonicRandB &= RAND_B_MASK;
        state.monotonicRandA++;
      }
      if (state.monotonicRandA > RAND_A_MASK) {
        state.lastTimestampMs++;
        reseed = true;
      }
    }
    if (reseed) {
      state.monotonicRandA = freshRandA & (RAND_A_MASK >> 1);
      state.monotonicRandB = freshRandB;
    }
    randA = state.monotonicRandA;
    randB = state.monotonicRandB;
    break;
  }

  default:
    break;
  }

  uint8_t uuidBytes[16];
  for (int i = 0; i < 6; i++)
    uuidBytes[i] = static_cast<uint8_t>(state.lastTimestampMs >> (40 - 8 * i));
  uuidBytes[6] = static_cast<uint8_t>(0x70 | ((randA >> 8) & 0x0F));
  uuidBytes[7] = static_cast<uint8_t>(randA & 0xFF);
  uuidBytes[8] = static_cast<uint8_t>(0x80 | ((randB >> 56) & 0x3F));
  for (int i = 9; i < 16; i++)
    uuidBytes[i] = static_cast<uint8_t>(randB >> (8 * (15 - i)));

  std::vector<char> result = FormatUuidText(uuidBytes, uppercase, hyphens);
  binaryValue.assign(uuidBytes, uuidBytes + sizeof(uuidBytes));
  SecureZeroMemory(uuidBytes, sizeof(uuidBytes));
  return result;
}

}
