#include "text_formats.h"
#include "../../../config/AppConfig.h"
#include "cancellation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <windows.h>

namespace CSPRNG {

std::vector<char> GenerateRandomCharacters(const std::vector<uint8_t> &randomBytes,
                                           int length, bool includeNumbers,
                                           bool includeUppercase,
                                           bool includeLowercase,
                                           bool includeSpecial,
                                           std::vector<uint8_t> &binaryValue) {
  binaryValue.clear();
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
  binaryValue.reserve(length);

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
      Crypto::SecureClearVector(binaryValue);
      throw std::runtime_error(
          "Random Characters: too many random draws were rejected. Please "
          "try again.");
    }

    size_t characterIndex = draw % characterSetSize;
    result.push_back(characterSet[characterIndex]);
    binaryValue.push_back(static_cast<uint8_t>(characterIndex));
  }

  return result;
}

static constexpr uint32_t THREE_BYTE_DRAW_RANGE = 1u << 24;
static constexpr uint32_t DIGIT_COUNT = 10;
static constexpr char NO_NUMBER_IN_SLOT = '\0';
static constexpr uint8_t NO_NUMBER_IN_SLOT_BYTE = 0xFF;
static constexpr int CANCEL_CHECK_MASK = 0x3FF;
static_assert(AppConfig::WORD_LIST_MAX_WORDS <= THREE_BYTE_DRAW_RANGE);
static_assert(AppConfig::PASSPHRASE_MAX_NUMBERS_PER_WORD == 2,
              "each word has one number slot before it and one after it");

static uint32_t DrawBelow(const std::vector<uint8_t> &randomBytes,
                          size_t &byteIndex, uint32_t range) {
  const uint32_t acceptBelow =
      THREE_BYTE_DRAW_RANGE - (THREE_BYTE_DRAW_RANGE % range);
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
  return draw % range;
}

double PassphraseNumberBits(int wordCount, int numberCount) {
  if (wordCount <= 0 || numberCount <= 0)
    return 0.0;
  const double slotCount = static_cast<double>(wordCount) *
                           AppConfig::PASSPHRASE_MAX_NUMBERS_PER_WORD;
  const double numbers = std::min(static_cast<double>(numberCount), slotCount);
  const double slotChoiceBits =
      (std::lgamma(slotCount + 1.0) - std::lgamma(numbers + 1.0) -
       std::lgamma(slotCount - numbers + 1.0)) /
      std::log(2.0);
  return numbers * std::log2(static_cast<double>(DIGIT_COUNT)) + slotChoiceBits;
}

std::vector<char> GeneratePassphrase(const std::vector<uint8_t> &randomBytes,
                                     int wordCount, int numberCount,
                                     const std::string &separator,
                                     const std::vector<std::string> &wordList,
                                     std::vector<uint8_t> &binaryValue) {
  binaryValue.clear();
  if (wordList.empty() || randomBytes.empty() || wordCount <= 0)
    throw std::runtime_error("Passphrase: no word list or random data.");
  if (wordList.size() > THREE_BYTE_DRAW_RANGE)
    throw std::runtime_error("Passphrase: the word list is too large.");
  const size_t slotCount = static_cast<size_t>(wordCount) *
                           AppConfig::PASSPHRASE_MAX_NUMBERS_PER_WORD;
  if (numberCount < 0 || static_cast<size_t>(numberCount) > slotCount)
    throw std::runtime_error("Passphrase: too many numbers for the word count.");
  const uint32_t wordListSize = static_cast<uint32_t>(wordList.size());

  const size_t WORD_INDEX_BYTES = sizeof(uint32_t);
  binaryValue.reserve(static_cast<size_t>(wordCount) * WORD_INDEX_BYTES +
                      (numberCount > 0 ? slotCount : 0));
  std::vector<uint32_t> wordIndices;
  std::vector<uint32_t> shuffledSlots;
  std::vector<char> numberInSlot;
  std::string result;
  size_t byteIndex = 0;
  auto wipeWorkingState = [&]() {
    Crypto::SecureClearVector(wordIndices);
    Crypto::SecureClearVector(shuffledSlots);
    Crypto::SecureClearVector(numberInSlot);
    SecureZeroMemory(result.data(), result.size());
  };

  try {
    wordIndices.reserve(static_cast<size_t>(wordCount));
    for (int i = 0; i < wordCount; i++) {
      if ((i & CANCEL_CHECK_MASK) == 0)
        ThrowIfGenerationCancelled();
      const uint32_t wordIndex = DrawBelow(randomBytes, byteIndex, wordListSize);
      wordIndices.push_back(wordIndex);
      for (size_t b = 0; b < WORD_INDEX_BYTES; b++)
        binaryValue.push_back(static_cast<uint8_t>(wordIndex >> (8 * b)));
    }

    if (numberCount > 0) {
      shuffledSlots.resize(slotCount);
      for (size_t slot = 0; slot < slotCount; slot++)
        shuffledSlots[slot] = static_cast<uint32_t>(slot);
      numberInSlot.assign(slotCount, NO_NUMBER_IN_SLOT);
      for (int i = 0; i < numberCount; i++) {
        if ((i & CANCEL_CHECK_MASK) == 0)
          ThrowIfGenerationCancelled();
        const size_t pick =
            i + DrawBelow(randomBytes, byteIndex,
                          static_cast<uint32_t>(slotCount - i));
        std::swap(shuffledSlots[i], shuffledSlots[pick]);
        numberInSlot[shuffledSlots[i]] = static_cast<char>(
            '0' + DrawBelow(randomBytes, byteIndex, DIGIT_COUNT));
      }
      for (char number : numberInSlot)
        binaryValue.push_back(number == NO_NUMBER_IN_SLOT
                                  ? NO_NUMBER_IN_SLOT_BYTE
                                  : static_cast<uint8_t>(number - '0'));
    }

    for (int i = 0; i < wordCount; i++) {
      if (i > 0)
        result += separator;
      const size_t slotBefore =
          static_cast<size_t>(i) * AppConfig::PASSPHRASE_MAX_NUMBERS_PER_WORD;
      if (numberCount > 0 && numberInSlot[slotBefore] != NO_NUMBER_IN_SLOT)
        result += numberInSlot[slotBefore];
      result += wordList[wordIndices[i]];
      if (numberCount > 0 && numberInSlot[slotBefore + 1] != NO_NUMBER_IN_SLOT)
        result += numberInSlot[slotBefore + 1];
    }
  } catch (...) {
    wipeWorkingState();
    Crypto::SecureClearVector(binaryValue);
    throw;
  }

  std::vector<char> passphrase(result.begin(), result.end());
  wipeWorkingState();
  return passphrase;
}

}
