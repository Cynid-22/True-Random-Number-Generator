#include "raw_data_format.h"
#include "cancellation.h"
#include "../../../config/AppConfig.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <windows.h>

namespace CSPRNG {

static_assert(RAW_ENCODING_COUNT == AppState::RAW_DATA_ENCODING_COUNT,
              "AppState Raw Data grouping arrays must cover every encoding");

static const char *RawEncodingAlphabet(int encoding) {
  switch (encoding) {
  case RAW_BINARY:
    return "01";
  case RAW_OCTAL:
    return "01234567";
  case RAW_DECIMAL:
    return "0123456789";
  case RAW_DUODECIMAL:
    return "0123456789AB";
  case RAW_BASE32:
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  case RAW_CROCKFORD32:
    return "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
  case RAW_BASE36:
    return "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  case RAW_BASE45:
    return "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
  case RAW_BASE58:
    return "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
  case RAW_BASE62:
    return "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  case RAW_BASE64:
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  case RAW_BASE64URL:
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  case RAW_BASE85:
    return "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
           "!#$%&()*+-;<=>?@^_`{|}~";
  default:
    return "0123456789abcdef";
  }
}

static int RawEncodingBitsPerCharacter(int encoding) {
  switch (encoding) {
  case RAW_BINARY:
    return 1;
  case RAW_OCTAL:
    return 3;
  case RAW_HEX:
    return 4;
  case RAW_BASE32:
  case RAW_CROCKFORD32:
    return 5;
  case RAW_BASE64:
  case RAW_BASE64URL:
    return 6;
  default:
    return 0;
  }
}

bool RawEncodingHasSingleCaseLetters(int encoding) {
  switch (encoding) {
  case RAW_HEX:
  case RAW_DUODECIMAL:
  case RAW_BASE32:
  case RAW_CROCKFORD32:
  case RAW_BASE36:
  case RAW_BASE45:
    return true;
  default:
    return false;
  }
}

uint64_t RawDataBitCount(int amount, int unit) {
  return (unit == RAW_DATA_UNIT_BITS) ? static_cast<uint64_t>(amount)
                                      : static_cast<uint64_t>(amount) * 8;
}

RawDataLayout GetRawDataLayout(int encoding, uint64_t bits) {
  RawDataLayout layout{0, 0, 0.0, false};
  const uint64_t wholeBytes = (bits + 7) / 8;

  switch (encoding) {
  case RAW_BINARY:
  case RAW_OCTAL:
  case RAW_CROCKFORD32: {
    const uint64_t bitsPerCharacter =
        static_cast<uint64_t>(RawEncodingBitsPerCharacter(encoding));
    layout.characterCount = (bits + bitsPerCharacter - 1) / bitsPerCharacter;
    layout.randomByteCount = (layout.characterCount * bitsPerCharacter + 7) / 8;
    layout.bitsCarried =
        static_cast<double>(layout.characterCount * bitsPerCharacter);
    return layout;
  }

  case RAW_DECIMAL:
  case RAW_DUODECIMAL:
  case RAW_BASE36:
  case RAW_BASE58:
  case RAW_BASE62: {
    const double bitsPerDigit =
        std::log2(static_cast<double>(strlen(RawEncodingAlphabet(encoding))));
    layout.usesRandomDigits = true;
    layout.characterCount = static_cast<uint64_t>(
        std::ceil(static_cast<double>(bits) / bitsPerDigit));
    layout.bitsCarried = static_cast<double>(layout.characterCount) * bitsPerDigit;
    layout.randomByteCount = layout.characterCount + layout.characterCount / 4 +
                             AppConfig::REJECTION_MIN_BYTES_PER_ITEM;
    return layout;
  }

  case RAW_BASE32:
    layout.characterCount = (wholeBytes + 4) / 5 * 8;
    break;
  case RAW_BASE45:
    layout.characterCount = wholeBytes / 2 * 3 + (wholeBytes % 2) * 2;
    break;
  case RAW_BASE64:
    layout.characterCount = (wholeBytes + 2) / 3 * 4;
    break;
  case RAW_BASE64URL:
    layout.characterCount = (wholeBytes * 8 + 5) / 6;
    break;
  case RAW_BASE85:
    layout.characterCount =
        wholeBytes / 4 * 5 + (wholeBytes % 4 ? wholeBytes % 4 + 1 : 0);
    break;
  default:
    layout.characterCount = wholeBytes * 2;
    break;
  }
  layout.randomByteCount = wholeBytes;
  layout.bitsCarried = static_cast<double>(wholeBytes * 8);
  return layout;
}

class GroupedCharacterWriter {
public:
  GroupedCharacterWriter(std::vector<char> &output, uint64_t groupSize)
      : m_output(output), m_groupSize(groupSize) {}

  void Write(char character) {
    if (m_groupSize && m_writtenCount > 0 && m_writtenCount % m_groupSize == 0)
      m_output.push_back(' ');
    m_output.push_back(character);
    if ((++m_writtenCount & 0xFFFF) == 0)
      ThrowIfGenerationCancelled();
  }

private:
  std::vector<char> &m_output;
  uint64_t m_groupSize;
  uint64_t m_writtenCount = 0;
};

static void BuildCasedAlphabet(int encoding, bool uppercase, char *alphabet,
                               size_t capacity) {
  const char *source = RawEncodingAlphabet(encoding);
  const bool applyCase = RawEncodingHasSingleCaseLetters(encoding);
  for (size_t i = 0; source[i] != '\0' && i + 1 < capacity; i++) {
    const unsigned char character = static_cast<unsigned char>(source[i]);
    alphabet[i] = applyCase ? static_cast<char>(uppercase ? toupper(character)
                                                          : tolower(character))
                            : source[i];
  }
}

static void WriteBitStream(GroupedCharacterWriter &writer,
                           const std::vector<uint8_t> &randomBytes,
                           const RawDataLayout &layout, int encoding,
                           int bitsPerCharacter, const char *alphabet) {
  const uint64_t dataCharacterCount =
      (encoding == RAW_BASE32 || encoding == RAW_BASE64)
          ? (layout.randomByteCount * 8 + bitsPerCharacter - 1) / bitsPerCharacter
          : layout.characterCount;
  const uint32_t mask = (1u << bitsPerCharacter) - 1;
  uint32_t bitBuffer = 0;
  int bufferedBitCount = 0;
  uint64_t byteIndex = 0;
  for (uint64_t c = 0; c < dataCharacterCount; c++) {
    while (bufferedBitCount < bitsPerCharacter) {
      bitBuffer = (bitBuffer << 8) |
                  (byteIndex < layout.randomByteCount ? randomBytes[byteIndex] : 0u);
      byteIndex++;
      bufferedBitCount += 8;
    }
    bufferedBitCount -= bitsPerCharacter;
    writer.Write(alphabet[(bitBuffer >> bufferedBitCount) & mask]);
    bitBuffer &= (1u << bufferedBitCount) - 1;
  }
  for (uint64_t c = dataCharacterCount; c < layout.characterCount; c++)
    writer.Write('=');
}

static void WriteBase45(GroupedCharacterWriter &writer,
                        const std::vector<uint8_t> &randomBytes,
                        const RawDataLayout &layout, const char *alphabet) {
  uint64_t i = 0;
  for (; i + 1 < layout.randomByteCount; i += 2) {
    uint32_t pairValue = randomBytes[i] * 256u + randomBytes[i + 1];
    writer.Write(alphabet[pairValue % 45]);
    writer.Write(alphabet[(pairValue / 45) % 45]);
    writer.Write(alphabet[pairValue / 2025]);
  }
  if (i < layout.randomByteCount) {
    uint32_t lastByte = randomBytes[i];
    writer.Write(alphabet[lastByte % 45]);
    writer.Write(alphabet[lastByte / 45]);
  }
}

static void WriteBase85(GroupedCharacterWriter &writer,
                        const std::vector<uint8_t> &randomBytes,
                        const RawDataLayout &layout, const char *alphabet) {
  char block[5];
  for (uint64_t i = 0; i < layout.randomByteCount; i += 4) {
    const uint64_t blockByteCount = std::min<uint64_t>(4, layout.randomByteCount - i);
    uint32_t blockValue = 0;
    for (uint64_t b = 0; b < 4; b++)
      blockValue = (blockValue << 8) | (b < blockByteCount ? randomBytes[i + b] : 0u);
    for (int d = 4; d >= 0; d--) {
      block[d] = alphabet[blockValue % 85];
      blockValue /= 85;
    }
    for (uint64_t d = 0; d <= blockByteCount; d++)
      writer.Write(block[d]);
  }
  SecureZeroMemory(block, sizeof(block));
}

static void WriteRandomDigits(GroupedCharacterWriter &writer,
                              const std::vector<uint8_t> &randomBytes,
                              const RawDataLayout &layout, const char *alphabet) {
  const unsigned radix = static_cast<unsigned>(strlen(alphabet));
  const unsigned acceptBelow = 256 - 256 % radix;
  uint64_t byteIndex = 0;
  for (uint64_t c = 0; c < layout.characterCount; c++) {
    unsigned draw;
    do {
      if (byteIndex >= layout.randomByteCount)
        throw std::runtime_error(
            "Raw Data: too many random draws were rejected. Please try "
            "again.");
      draw = randomBytes[byteIndex++];
    } while (draw >= acceptBelow);
    writer.Write(alphabet[draw % radix]);
  }
}

std::vector<char> GenerateRawData(const std::vector<uint8_t> &randomBytes,
                                  int amount, int unit, int encoding,
                                  int groupSize, bool uppercase) {
  if (encoding < 0 || encoding >= RAW_ENCODING_COUNT)
    encoding = RAW_HEX;

  const RawDataLayout layout =
      GetRawDataLayout(encoding, RawDataBitCount(amount, unit));
  if (randomBytes.size() < layout.randomByteCount)
    throw std::runtime_error("Raw Data: not enough random bytes.");

  char alphabet[96] = {};
  BuildCasedAlphabet(encoding, uppercase, alphabet, sizeof(alphabet));
  const uint64_t group = groupSize > 0 ? static_cast<uint64_t>(groupSize) : 0;

  std::vector<char> output;
  output.reserve(layout.characterCount +
                 (group ? layout.characterCount / group : 0));
  GroupedCharacterWriter writer(output, group);

  try {
    const int bitsPerCharacter = RawEncodingBitsPerCharacter(encoding);
    if (bitsPerCharacter > 0)
      WriteBitStream(writer, randomBytes, layout, encoding, bitsPerCharacter,
                     alphabet);
    else if (encoding == RAW_BASE45)
      WriteBase45(writer, randomBytes, layout, alphabet);
    else if (encoding == RAW_BASE85)
      WriteBase85(writer, randomBytes, layout, alphabet);
    else
      WriteRandomDigits(writer, randomBytes, layout, alphabet);
  } catch (...) {
    Crypto::SecureClearVector(output);
    throw;
  }

  return output;
}

}
