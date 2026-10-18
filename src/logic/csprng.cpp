#include "csprng.h"
#include "../../config/AppConfig.h"
#include "../core/app_state.h"
#include "../crypto/aes.h"
#include "../crypto/chacha20.h"
#include "../crypto/hkdf.h"
#include "../crypto/secure_mem.h"
#include "../crypto/sha512.h"
#include "../logging/logger.h"
#include "logic.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <windows.h> // For SecureZeroMemory


namespace CSPRNG {

static_assert(RAW_ENCODING_COUNT == AppState::kRawEncodingCount,
              "AppState Raw Data grouping arrays must cover every encoding");

//=============================================================================
// ENTROPY SERIALIZATION
//=============================================================================

std::vector<uint8_t>
SerializeEntropyData(const std::vector<Entropy::EntropyDataPoint> &data) {

  // Each data point: 8 bytes timestamp + 8 bytes value = 16 bytes
  std::vector<uint8_t> result;
  result.reserve(data.size() * 16);

  for (const auto &point : data) {
    // Add timestamp (little-endian)
    for (int i = 0; i < 8; i++) {
      result.push_back(static_cast<uint8_t>(point.timestamp >> (i * 8)));
    }
    // Add value (little-endian)
    for (int i = 0; i < 8; i++) {
      result.push_back(static_cast<uint8_t>(point.value >> (i * 8)));
    }
  }

  return result;
}

//=============================================================================
// CORE GENERATION LOGIC
//=============================================================================

std::vector<uint8_t>
GenerateRandomBytes(const std::vector<Entropy::EntropyDataPoint> &entropyData,
                    size_t numBytes, uint32_t attempt) {

  // 1. Serialize ENTIRE entropy pool
  std::vector<uint8_t> entropyBytes = SerializeEntropyData(entropyData);

  // Log entropy pool state
  // (The TRUE RANDOMNESS / PSEUDO-RANDOM mode is decided by the caller, see
  // GenerateOutput, using the same rule as the UI.)
  Logger::Log(Logger::Level::INFO, "CSPRNG",
              "GenerateRandomBytes: %zu data points, %zu entropy bytes, "
              "requesting %zu output bytes (attempt %u)",
              entropyData.size(), entropyBytes.size(), numBytes, attempt);

  // 2. QUAD-LAYER ARCHITECTURE (Maximum Security)
  // Layer 1: ChaCha20 Masking (Stream Cipher)
  // Layer 2: Entropy Injection (Information Theoretic XOR)
  // Layer 3: AES-256 Transformation (Block Cipher)
  // Layer 4: ChaCha20 Final Whitening (Stream Cipher)

  //-------------------------------------------------------------------------
  // LAYER 1: ChaCha20 Masking
  //-------------------------------------------------------------------------
  // Hash pool + params to get master seed
  Crypto::SHA512::Hash masterSeed = Crypto::SHA512::Compute(entropyBytes);

  std::ostringstream infoStream;
  infoStream << "TRNG-L1|Len:" << numBytes << "|Fmt:" << g_state.outputFormat
             << "|";
  // Add format-specific params to context to ensure avalanche
  switch (g_state.outputFormat) {
  case 0:
    infoStream << "D:" << g_state.decimalDigits;
    break;
  case 1:
    infoStream << "I:" << g_state.integerMin << ":" << g_state.integerMax;
    break;
  case 3:
    infoStream << "C:" << g_state.customLength;
    break;
  case 4:
    infoStream << "U:" << g_state.bitByteUnit << "A:" << g_state.bitByteAmount
               << "E:" << g_state.bitByteFormat;
    break;
  case 5:
    infoStream << "W:" << g_state.passphraseWordCount;
    break;
  case 6:
    infoStream << "O:" << g_state.otpInputMode;
    break;
  }
  if (g_state.outputFormat != 6)
    infoStream << "|Q:" << g_state.outputQuantity;
  infoStream << "|T:" << Entropy::GetNanosecondTimestamp();
  // Retries must not depend on the clock having ticked: the attempt number
  // guarantees a distinct key/nonce. Omitted on the first attempt so the
  // normal path is unchanged.
  if (attempt > 0)
    infoStream << "|R:" << attempt;
  std::string infoStr = infoStream.str();
  std::vector<uint8_t> info(infoStr.begin(), infoStr.end());

  std::vector<uint8_t> salt;
  std::vector<uint8_t> keyMaterial = Crypto::HKDF::DeriveKey(
      std::vector<uint8_t>(masterSeed.begin(), masterSeed.end()), salt, info,
      44);

  Crypto::ChaCha20::Key key1;
  Crypto::ChaCha20::Nonce nonce1;
  std::copy(keyMaterial.begin(), keyMaterial.begin() + 32, key1.begin());
  std::copy(keyMaterial.begin() + 32, keyMaterial.begin() + 44, nonce1.begin());

  std::vector<uint8_t> stream1 =
      Crypto::ChaCha20::GenerateStream(key1, nonce1, numBytes);

  //-------------------------------------------------------------------------
  // LAYER 2: Entropy Injection (XOR Fold)
  //-------------------------------------------------------------------------
  // Fold full entropy pool into stream1
  // Fold full entropy pool into stream1
  // FIX: Iterate over whichever is larger to ensure ALL entropy is mixed in
  // If pool > stream, we wrap around stream index
  // If stream > pool, we wrap around pool index
  if (!entropyBytes.empty()) {
    size_t loopCount = std::max(stream1.size(), entropyBytes.size());
    for (size_t i = 0; i < loopCount; i++) {
      stream1[i % stream1.size()] ^= entropyBytes[i % entropyBytes.size()];
    }
  }
  // Logger::Log(Logger::Level::DEBUG, "CSPRNG", "Layer 2: Entropy Fold
  // Complete");

  //-------------------------------------------------------------------------
  // LAYER 3: AES-256 Transformation
  //-------------------------------------------------------------------------
  // Use Hash(Stream1) to key AES-256 for transforming Stream1
  Crypto::SHA512::Hash s1Hash = Crypto::SHA512::Compute(stream1);

  std::vector<uint8_t> aesKey(s1Hash.begin(),
                              s1Hash.begin() + 32); // 32 bytes Key
  std::vector<uint8_t> aesIV(s1Hash.begin() + 32,
                             s1Hash.begin() + 48); // 16 bytes IV

  std::vector<uint8_t> stream3 =
      Crypto::AES256::EncryptCTR(aesKey, aesIV, stream1);
  // Logger::Log(Logger::Level::DEBUG, "CSPRNG", "Layer 3: AES-256
  // Transformation Complete");

  //-------------------------------------------------------------------------
  // LAYER 4: ChaCha20 Final Whitening
  //-------------------------------------------------------------------------
  // Use Hash(Stream3) to seed final ChaCha20 pass
  // This ensures AES bias (if any existed, which shouldn't) is washed away
  Crypto::SHA512::Hash s3Hash = Crypto::SHA512::Compute(stream3);

  std::vector<uint8_t> info4 = {'L', 'A', 'Y', 'E', 'R', '4'};
  std::vector<uint8_t> key4Mat = Crypto::HKDF::DeriveKey(
      std::vector<uint8_t>(s3Hash.begin(), s3Hash.end()), salt, info4, 44);

  Crypto::ChaCha20::Key key4;
  Crypto::ChaCha20::Nonce nonce4;
  std::copy(key4Mat.begin(), key4Mat.begin() + 32, key4.begin());
  std::copy(key4Mat.begin() + 32, key4Mat.begin() + 44, nonce4.begin());

  std::vector<uint8_t> result =
      Crypto::ChaCha20::GenerateStream(key4, nonce4, numBytes);
  // Logger::Log(Logger::Level::DEBUG, "CSPRNG", "Layer 4: Final Whitening
  // Complete");

  // 7. Debug Logging REMOVED for Security

  // 8. Secure Cleanup
  SecureZeroMemory(entropyBytes.data(), entropyBytes.size());
  SecureZeroMemory(masterSeed.data(), masterSeed.size());
  SecureZeroMemory(keyMaterial.data(), keyMaterial.size());
  SecureZeroMemory(stream1.data(), stream1.size());
  SecureZeroMemory(s1Hash.data(), s1Hash.size());
  SecureZeroMemory(aesKey.data(), aesKey.size());
  SecureZeroMemory(stream3.data(), stream3.size());
  SecureZeroMemory(s3Hash.data(), s3Hash.size());
  SecureZeroMemory(key4Mat.data(), key4Mat.size());

  return result;
}

// Legacy wrappers no longer needed but kept empty/removed to avoid link errors
// if accessed We implemented the logic directly in GenerateRandomBytes
std::vector<uint8_t>
ConsolidateEntropy(const std::vector<uint8_t> &entropyBytes,
                   size_t outputBytes) {
  // Not used anymore
  return {};
}

std::vector<uint8_t> ExpandEntropy(const std::vector<uint8_t> &entropyBytes,
                                   size_t outputBytes) {
  // Not used anymore
  return {};
}

//=============================================================================
// FORMAT-SPECIFIC GENERATORS
//=============================================================================

std::vector<char> GenerateDecimal(const std::vector<uint8_t> &randomBytes,
                                  int digits) {
  if (randomBytes.empty() || digits <= 0) {
    std::string s = "0.0";
    return std::vector<char>(s.begin(), s.end());
  }

  std::ostringstream oss;
  oss << "0.";

  // Rejection Sampling for base 10
  // Range: 10. Source: 2 bytes (0-65535).
  // Limit: 65535 - (65535 % 10) = 65530.
  // Accept if val < 65530.
  const uint16_t LIMIT = 65530;

  size_t byteIdx = 0;

  for (int i = 0; i < digits; i++) {
    if ((i & 0x3FFF) == 0 && g_state.cancelGeneration) throw std::runtime_error("Generation cancelled by user.");
    uint16_t val = 0;
    bool found = false;

    while (byteIdx + 1 < randomBytes.size()) {
      val = (static_cast<uint16_t>(randomBytes[byteIdx]) << 8) |
            randomBytes[byteIdx + 1];
      byteIdx += 2;

      if (val < LIMIT) {
        found = true;
        break;
      }
      // Reject and try next 2 bytes
    }

    if (!found) {
      // Should not happen with 4x buffer, but handle safe
      val = 0;
    }

    int digit = val % 10;
    oss << digit;
  }

  std::string s = oss.str();
  std::vector<char> result(s.begin(), s.end());
  SecureZeroMemory(s.data(), s.size());
  return result;
}

std::vector<char> GenerateInteger(const std::vector<uint8_t> &randomBytes,
                                  int64_t min, int64_t max, bool &exhausted) {
  exhausted = false;
  if (min > max)
    std::swap(min, max);

  // Unsigned arithmetic: max - min can exceed INT64_MAX. A range of 0 means
  // the full 2^64 span [INT64_MIN, INT64_MAX] (max - min + 1 wrapped).
  uint64_t range = static_cast<uint64_t>(max) - static_cast<uint64_t>(min) + 1;
  bool fullRange = (range == 0);

  // Rejection threshold: 2^64 mod range. Draws below it are rejected so the
  // accepted values [threshold, 2^64) number an exact multiple of range,
  // making randVal % range uniform. Draws are always 64-bit so this formula
  // holds for every range; for ranges up to 2^32 rejection is < 2^-32.
  uint64_t threshold = fullRange ? 0 : (0 - range) % range;

  uint64_t randVal = 0;
  bool found = false;
  size_t byteIdx = 0;

  // Rejection Sampling Loop (8 bytes per draw)
  while (byteIdx + 8 <= randomBytes.size()) {
    randVal = 0;
    for (size_t i = 0; i < 8; i++) {
      randVal = (randVal << 8) | randomBytes[byteIdx++];
    }

    if (randVal >= threshold) {
      found = true;
      break;
    }
  }

  if (!found) {
    // Never fall back to a rejected draw (it would be biased toward the low
    // end). The caller retries with fresh bytes.
    exhausted = true;
    return {};
  }

  // Offset in unsigned space to avoid signed overflow, then convert back.
  uint64_t offset = fullRange ? randVal : randVal % range;
  int64_t resultVal =
      static_cast<int64_t>(static_cast<uint64_t>(min) + offset);

  std::string s = std::to_string(resultVal);
  std::vector<char> result(s.begin(), s.end());
  SecureZeroMemory(s.data(), s.size());
  return result;
}

std::vector<char> GenerateCustomString(const std::vector<uint8_t> &randomBytes,
                                       int length, bool includeNumbers,
                                       bool includeUppercase,
                                       bool includeLowercase,
                                       bool includeSpecial) {

  // Build character set
  std::string charset;
  if (includeNumbers)
    charset += "0123456789";
  if (includeUppercase)
    charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (includeLowercase)
    charset += "abcdefghijklmnopqrstuvwxyz";
  if (includeSpecial)
    charset += "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

  if (charset.empty() || randomBytes.empty() || length <= 0)
    return {};

  std::vector<char> result;
  result.reserve(length);

  // Rejection sampling set up
  size_t setSize = charset.size();
  uint16_t limit = 65535 - (65535 % static_cast<uint16_t>(setSize));

  size_t byteIdx = 0;

  for (int i = 0; i < length; i++) {
    if ((i & 0x3FFF) == 0 && g_state.cancelGeneration) throw std::runtime_error("Generation cancelled by user.");
    uint16_t val = 0;
    bool found = false;

    while (byteIdx + 1 < randomBytes.size()) {
      val = (static_cast<uint16_t>(randomBytes[byteIdx]) << 8) |
            randomBytes[byteIdx + 1];
      byteIdx += 2;

      if (val < limit) {
        found = true;
        break;
      }
    }

    if (!found)
      val = 0;

    size_t charIdx = val % setSize;
    result.push_back(charset[charIdx]);
  }

  return result;
}

//-----------------------------------------------------------------------------
// Raw Data encodings
//-----------------------------------------------------------------------------

// Alphabet of each RawEncoding
static const char *RawAlphabet(int encoding) {
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
  default: // RAW_HEX
    return "0123456789abcdef";
  }
}

// Bits per character for power-of-two bases (read as a bit stream), else 0
static int RawPackedBits(int encoding) {
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

bool RawHasSingleCaseLetters(int encoding) {
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

RawLayout GetRawLayout(int encoding, uint64_t bits) {
  RawLayout layout{0, 0, 0.0, false};
  const uint64_t wholeBytes = (bits + 7) / 8;

  switch (encoding) {
  case RAW_BINARY:
  case RAW_OCTAL:
  case RAW_CROCKFORD32: { // Exact bits: just enough characters, all random
    const uint64_t k = static_cast<uint64_t>(RawPackedBits(encoding));
    layout.chars = (bits + k - 1) / k;
    layout.bytes = (layout.chars * k + 7) / 8;
    layout.bitsCarried = static_cast<double>(layout.chars * k);
    return layout;
  }

  case RAW_DECIMAL:
  case RAW_DUODECIMAL:
  case RAW_BASE36:
  case RAW_BASE58:
  case RAW_BASE62: { // Uniform random digits holding at least `bits`
    const double bitsPerDigit =
        std::log2(static_cast<double>(strlen(RawAlphabet(encoding))));
    layout.digits = true;
    layout.chars =
        static_cast<uint64_t>(std::ceil(static_cast<double>(bits) / bitsPerDigit));
    layout.bitsCarried = static_cast<double>(layout.chars) * bitsPerDigit;
    // One byte per draw, plus headroom for rejected draws (at most 24 of
    // every 256 values, for Base 58)
    layout.bytes = layout.chars + layout.chars / 4 +
                   AppConfig::REJECTION_MIN_BYTES_PER_ITEM;
    return layout;
  }

  // Byte encodings: whole bytes, standard output length
  case RAW_BASE32: // 5 bytes -> 8 chars, '=' padded
    layout.chars = (wholeBytes + 4) / 5 * 8;
    break;
  case RAW_BASE45: // 2 bytes -> 3 chars, a final byte -> 2 chars
    layout.chars = wholeBytes / 2 * 3 + (wholeBytes % 2) * 2;
    break;
  case RAW_BASE64: // 3 bytes -> 4 chars, '=' padded
    layout.chars = (wholeBytes + 2) / 3 * 4;
    break;
  case RAW_BASE64URL: // Unpadded
    layout.chars = (wholeBytes * 8 + 5) / 6;
    break;
  case RAW_BASE85: // 4 bytes -> 5 chars, a final m bytes -> m + 1 chars
    layout.chars = wholeBytes / 4 * 5 + (wholeBytes % 4 ? wholeBytes % 4 + 1 : 0);
    break;
  default: // RAW_HEX
    layout.chars = wholeBytes * 2;
    break;
  }
  layout.bytes = wholeBytes;
  layout.bitsCarried = static_cast<double>(wholeBytes * 8);
  return layout;
}

std::vector<char> GenerateBitByte(const std::vector<uint8_t> &randomBytes,
                                  int amount, int unit, int encoding,
                                  int groupSize, bool uppercase) {
  if (encoding < 0 || encoding >= RAW_ENCODING_COUNT)
    encoding = RAW_HEX;

  const uint64_t bits = (unit == 0) ? static_cast<uint64_t>(amount)
                                    : static_cast<uint64_t>(amount) * 8;
  const RawLayout layout = GetRawLayout(encoding, bits);
  if (randomBytes.size() < layout.bytes)
    throw std::runtime_error("Raw Data: not enough random bytes.");

  // Alphabet, with single-case letters set to the chosen case
  char alphabet[96] = {};
  {
    const char *src = RawAlphabet(encoding);
    const bool setCase = RawHasSingleCaseLetters(encoding);
    for (size_t i = 0; src[i] != '\0' && i + 1 < sizeof(alphabet); i++) {
      const unsigned char c = static_cast<unsigned char>(src[i]);
      alphabet[i] = setCase ? static_cast<char>(uppercase ? toupper(c)
                                                          : tolower(c))
                            : src[i];
    }
  }
  const uint64_t group = groupSize > 0 ? static_cast<uint64_t>(groupSize) : 0;

  // Reserved up front (characters + separators) so the output never
  // reallocates and leaves unwiped copies behind
  std::vector<char> out;
  out.reserve(layout.chars + (group ? layout.chars / group : 0));
  uint64_t emitted = 0;
  auto emit = [&](char c) {
    if (group && emitted > 0 && emitted % group == 0)
      out.push_back(' ');
    out.push_back(c);
    if ((++emitted & 0xFFFF) == 0 && g_state.cancelGeneration)
      throw std::runtime_error("Generation cancelled by user.");
  };

  try {
    const int k = RawPackedBits(encoding);
    if (k > 0) {
      // Power-of-two base: read the bytes as a bit stream, k bits per
      // character. Byte encodings fill the last character with zero bits
      // (RFC 4648); exact-bit encodings have random bits for all of it.
      const uint64_t dataChars =
          (encoding == RAW_BASE32 || encoding == RAW_BASE64)
              ? (layout.bytes * 8 + k - 1) / k // Before '=' padding
              : layout.chars;
      const uint32_t mask = (1u << k) - 1;
      uint32_t acc = 0;
      int accBits = 0;
      uint64_t byteIdx = 0;
      for (uint64_t c = 0; c < dataChars; c++) {
        while (accBits < k) {
          acc = (acc << 8) |
                (byteIdx < layout.bytes ? randomBytes[byteIdx] : 0u);
          byteIdx++;
          accBits += 8;
        }
        accBits -= k;
        emit(alphabet[(acc >> accBits) & mask]);
        acc &= (1u << accBits) - 1;
      }
      acc = 0;
      for (uint64_t c = dataChars; c < layout.chars; c++)
        emit('=');

    } else if (encoding == RAW_BASE45) {
      // RFC 9285: each byte pair n = a*256 + b is written as c d e with
      // n = c + d*45 + e*45^2 (least significant first)
      uint64_t i = 0;
      for (; i + 1 < layout.bytes; i += 2) {
        uint32_t n = randomBytes[i] * 256u + randomBytes[i + 1];
        emit(alphabet[n % 45]);
        emit(alphabet[(n / 45) % 45]);
        emit(alphabet[n / 2025]);
      }
      if (i < layout.bytes) {
        uint32_t n = randomBytes[i];
        emit(alphabet[n % 45]);
        emit(alphabet[n / 45]);
      }

    } else if (encoding == RAW_BASE85) {
      // Each 4-byte big-endian block becomes 5 digits, most significant
      // first. A final m-byte block is zero-filled and keeps m + 1 digits.
      char block[5];
      for (uint64_t i = 0; i < layout.bytes; i += 4) {
        const uint64_t m = std::min<uint64_t>(4, layout.bytes - i);
        uint32_t v = 0;
        for (uint64_t b = 0; b < 4; b++)
          v = (v << 8) | (b < m ? randomBytes[i + b] : 0u);
        for (int d = 4; d >= 0; d--) {
          block[d] = alphabet[v % 85];
          v /= 85;
        }
        for (uint64_t d = 0; d <= m; d++)
          emit(block[d]);
      }
      SecureZeroMemory(block, sizeof(block));

    } else {
      // Uniform random digits: rejection sampling on single bytes, so every
      // digit is equally likely. A rejected byte is never used.
      const unsigned radix = static_cast<unsigned>(strlen(alphabet));
      const unsigned limit = 256 - 256 % radix; // Accept bytes below this
      uint64_t byteIdx = 0;
      for (uint64_t c = 0; c < layout.chars; c++) {
        unsigned v;
        do {
          if (byteIdx >= layout.bytes)
            throw std::runtime_error(
                "Raw Data: too many random draws were rejected. Please try "
                "again.");
          v = randomBytes[byteIdx++];
        } while (v >= limit);
        emit(alphabet[v % radix]);
      }
    }
  } catch (...) {
    Crypto::SecureClearVector(out);
    throw;
  }

  return out;
}

std::vector<char> GeneratePassphrase(const std::vector<uint8_t> &randomBytes,
                                     int wordCount,
                                     const std::string &separator,
                                     const std::vector<std::string> &wordlist) {

  std::string err = "[Error: Wordlist not loaded]";
  if (wordlist.empty() || randomBytes.empty() || wordCount <= 0) {
    return std::vector<char>(err.begin(), err.end());
  }

  std::string result;
  size_t bytesPerWord = 3; // 3 bytes = 24 bits, enough for ~16M words

  for (int i = 0; i < wordCount; i++) {
    if ((i & 0x3FF) == 0 && g_state.cancelGeneration) throw std::runtime_error("Generation cancelled by user.");
    // Build index from 3 random bytes
    size_t offset = i * bytesPerWord;
    uint32_t randomIndex = 0;

    for (size_t j = 0; j < bytesPerWord && (offset + j) < randomBytes.size();
         j++) {
      randomIndex = (randomIndex << 8) | randomBytes[offset + j];
    }

    // Select word using modular arithmetic
    size_t wordIndex = randomIndex % wordlist.size();

    if (i > 0)
      result += separator;
    result += wordlist[wordIndex];
  }

  std::vector<char> vec(result.begin(), result.end());
  SecureZeroMemory(result.data(), result.size());
  return vec;
}

std::vector<char> GenerateOTP(const std::vector<uint8_t> &randomBytes,
                              const std::string &message) {

  std::string result;
  result.reserve(message.size());
  size_t keyIdx = 0;

  for (char c : message) {
    // Validate ASCII (Printable 32-126)
    if (c < 32 || c > 126) {
      std::string err = "[Error: Message contains non-ASCII characters. Only "
                        "printable ASCII (32-126) allowed.]";
      return std::vector<char>(err.begin(), err.end());
    }

    // Rejection Sampling for Modulo 95 Uniformity
    // Range 95 (0-94). 256 / 95 = 2 rem 66.
    // Valid range: 0..189 (190 values = 2 * 95). Reject >= 190.
    uint8_t keyVal = 0;
    bool found = false;

    while (keyIdx < randomBytes.size()) {
      uint8_t b = randomBytes[keyIdx++];
      if (b < 190) {
        keyVal = b % 95;
        found = true;
        break;
      }
    }

    if (!found) {
      std::string err = "[Error: Insufficient entropy (rejection sampling "
                        "exhausted). Please retry.]";
      return std::vector<char>(err.begin(), err.end());
    }

    // ASCII Modulo Encryption: Cipher = (Msg + Key) % 95
    int msgVal = static_cast<int>(c) - 32;
    int cipherVal = (msgVal + static_cast<int>(keyVal)) % 95;
    result += static_cast<char>(cipherVal + 32);
  }

  std::vector<char> vec(result.begin(), result.end());
  SecureZeroMemory(result.data(), result.size());
  return vec;
}

std::vector<uint8_t> GenerateOTPFile(const std::vector<uint8_t> &randomBytes,
                                     const std::vector<uint8_t> &fileData) {

  std::vector<uint8_t> result(fileData.size());

  size_t keySize = randomBytes.size();
  for (size_t i = 0; i < fileData.size(); i++) {
    result[i] = fileData[i] ^ randomBytes[i % keySize];
  }

  return result;
}

// Format 16 UUID bytes as text (8-4-4-4-12 grouping when hyphenated)
static std::vector<char> FormatUUID(const uint8_t u[16], bool uppercase,
                                    bool hyphens) {
  const char *hex = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
  std::vector<char> result;
  result.reserve(36);
  for (int i = 0; i < 16; i++) {
    if (hyphens && (i == 4 || i == 6 || i == 8 || i == 10))
      result.push_back('-');
    result.push_back(hex[u[i] >> 4]);
    result.push_back(hex[u[i] & 0x0F]);
  }
  return result;
}

std::vector<char> GenerateUUIDv4(const std::vector<uint8_t> &randomBytes,
                                 bool uppercase, bool hyphens) {
  if (randomBytes.size() < 16)
    return {};

  uint8_t u[16];
  std::copy(randomBytes.begin(), randomBytes.begin() + 16, u);

  // RFC 9562: version 4 in the high nibble of byte 6, variant 10xx in byte 8
  u[6] = static_cast<uint8_t>((u[6] & 0x0F) | 0x40);
  u[8] = static_cast<uint8_t>((u[8] & 0x3F) | 0x80);

  std::vector<char> result = FormatUUID(u, uppercase, hyphens);
  SecureZeroMemory(u, sizeof(u));
  return result;
}

std::vector<char> GenerateUUIDv7(const std::vector<uint8_t> &randomBytes,
                                 bool uppercase, bool hyphens,
                                 const UUIDv7Options &options,
                                 UUIDv7State &state) {
  if (randomBytes.size() < 16)
    return {};

  // The 74 bits after the version/variant bits: rand_a (12) and rand_b (62)
  const uint64_t HI_MASK = 0xFFF;
  const uint64_t LO_MASK = (1ULL << 62) - 1;

  // 16 bytes: [0..9] a fresh random 74-bit field, [10..15] 48 bits for the
  // counter seed (Method 1) or the increment (Method 2)
  const uint8_t *r = randomBytes.data();
  uint64_t freshLo = 0;
  for (int i = 0; i < 8; i++)
    freshLo = (freshLo << 8) | r[i];
  freshLo &= LO_MASK;
  uint64_t freshHi = ((static_cast<uint64_t>(r[8]) << 8) | r[9]) & HI_MASK;
  uint64_t extra = 0;
  for (int i = 10; i < 16; i++)
    extra = (extra << 8) | r[i];

  uint64_t nowMs = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());

  // A new tick starts when the clock moves past the last timestamp used. If
  // it stays on (or steps back before) that millisecond, the timestamp is
  // kept so the batch never goes backwards.
  bool newTick = !state.haveLast || nowMs > state.lastMs;
  if (newTick) {
    state.lastMs = nowMs;
    state.haveLast = true;
  }

  uint64_t hi = freshHi;
  uint64_t lo = freshLo;

  switch (options.method) {
  case 0: { // Method 1: fixed bit-length dedicated counter
    // The counter takes the leftmost n bits of rand_a, continuing into
    // rand_b when n > 12; the bits after it stay fresh random per UUID.
    int n = std::min(std::max(options.counterBits,
                              AppConfig::UUIDV7_MIN_COUNTER_BITS),
                     AppConfig::UUIDV7_MAX_COUNTER_BITS);
    uint64_t counterMax = (1ULL << n) - 1;
    // Random seed with the top bit clear: room for at least 2^(n-1) UUIDs
    uint64_t seed = extra & (counterMax >> 1);

    if (newTick) {
      state.counter = seed;
    } else if (++state.counter > counterMax) {
      // Counter exhausted within this tick: move the timestamp forward
      state.lastMs++;
      state.counter = seed;
    }

    if (n <= 12) {
      int hiRandomBits = 12 - n;
      hi = (state.counter << hiRandomBits) |
           (freshHi & ((1ULL << hiRandomBits) - 1));
    } else {
      int loCounterBits = n - 12;
      int loRandomBits = 74 - n; // >= 32
      hi = state.counter >> loCounterBits;
      lo = ((state.counter & ((1ULL << loCounterBits) - 1)) << loRandomBits) |
           (freshLo & ((1ULL << loRandomBits) - 1));
    }
    break;
  }

  case 1: { // Method 2: monotonic random
    // All 74 bits are a randomly seeded counter, incremented in the least
    // significant position (rand_b) by a random amount in [1, 2^k].
    int k = std::min(std::max(options.incrementBits,
                              AppConfig::UUIDV7_MIN_INCREMENT_BITS),
                     AppConfig::UUIDV7_MAX_INCREMENT_BITS);
    uint64_t increment = (extra & ((1ULL << k) - 1)) + 1; // k <= 48 bits

    bool reseed = newTick;
    if (!newTick) {
      state.valueLo += increment; // < 2^63, cannot wrap
      if (state.valueLo > LO_MASK) {
        state.valueLo &= LO_MASK; // carry into rand_a
        state.valueHi++;
      }
      if (state.valueHi > HI_MASK) {
        // 74-bit value exhausted within this tick: move the timestamp forward
        state.lastMs++;
        reseed = true;
      }
    }
    if (reseed) {
      // Random seed with the top bit clear: at least 2^73 of headroom
      state.valueHi = freshHi & (HI_MASK >> 1);
      state.valueLo = freshLo;
    }
    hi = state.valueHi;
    lo = state.valueLo;
    break;
  }

  default: // None: all 74 bits fresh random; no order within a millisecond
    break;
  }

  uint8_t u[16];
  // unix_ts_ms: 48-bit big-endian
  for (int i = 0; i < 6; i++)
    u[i] = static_cast<uint8_t>(state.lastMs >> (40 - 8 * i));
  // version 7 + rand_a
  u[6] = static_cast<uint8_t>(0x70 | ((hi >> 8) & 0x0F));
  u[7] = static_cast<uint8_t>(hi & 0xFF);
  // variant 10xx + rand_b
  u[8] = static_cast<uint8_t>(0x80 | ((lo >> 56) & 0x3F));
  for (int i = 9; i < 16; i++)
    u[i] = static_cast<uint8_t>(lo >> (8 * (15 - i)));

  std::vector<char> result = FormatUUID(u, uppercase, hyphens);
  SecureZeroMemory(u, sizeof(u));
  return result;
}

//=============================================================================
// MAIN GENERATION FUNCTION
//=============================================================================

// Grow `out` to hold `extra` more bytes without leaving copies of its contents
// in freed heap memory (a plain vector reallocation would).
static void ReserveSecure(std::vector<char> &out, size_t extra) {
  size_t needed = out.size() + extra;
  if (needed <= out.capacity())
    return;
  std::vector<char> bigger;
  bigger.reserve(std::max(needed, out.capacity() * 2));
  bigger.insert(bigger.end(), out.begin(), out.end());
  Crypto::SecureClearVector(out);
  out.swap(bigger);
}

// Append one item to a list output: one item per line, optionally separated
// by a blank line and/or numbered "1. ", "2. ", ...
static void AppendListItem(std::vector<char> &out, size_t index,
                           size_t quantity, const std::vector<char> &item,
                           bool numbered, bool blankLine) {
  std::string prefix = numbered ? std::to_string(index + 1) + ". " : "";
  size_t separatorLen = (index == 0) ? 0 : (blankLine ? 2 : 1);

  if (index == 0) {
    // Size the buffer once from the first item so most lists never regrow
    size_t estimate = item.size() + 2 +
                      (numbered ? std::to_string(quantity).size() + 2 : 0);
    if (quantity <= (SIZE_MAX - 1) / estimate)
      ReserveSecure(out, estimate * quantity + 1);
  }

  ReserveSecure(out, separatorLen + prefix.size() + item.size());
  if (index > 0) {
    out.push_back('\n');
    if (blankLine)
      out.push_back('\n');
  }
  out.insert(out.end(), prefix.begin(), prefix.end());
  out.insert(out.end(), item.begin(), item.end());
}

GenerationResult GenerateOutput(GenerationMode mode) {
  GenerationResult result;
  result.success = false;
  result.mode = mode;
  result.entropyConsumed = 0.0f;
  result.rawBytesGenerated = 0;

  // Get enabled sources based on checkbox state (for new data filtering)
  std::set<Entropy::EntropySource> enabledSources;
  if (g_state.microphoneEnabled)
    enabledSources.insert(Entropy::EntropySource::Microphone);
  if (g_state.keystrokeEnabled)
    enabledSources.insert(Entropy::EntropySource::Keystroke);
  if (g_state.clockDriftEnabled)
    enabledSources.insert(Entropy::EntropySource::ClockDrift);
  if (g_state.cpuJitterEnabled)
    enabledSources.insert(Entropy::EntropySource::CpuJitter);
  if (g_state.mouseMovementEnabled)
    enabledSources.insert(Entropy::EntropySource::Mouse);

  // Get pooled entropy data
  // Note: If there's locked data, we include ALL locked data plus new data from
  // enabled sources
  std::vector<Entropy::EntropyDataPoint> pooledData;

  if (g_state.lockedDataTimestamp > 0) {
    // We have locked data - get everything
    pooledData = g_state.entropyPool.GetPooledData();
  } else {
    // No lock yet - respect current checkbox selections
    pooledData = g_state.entropyPool.GetPooledDataForSources(enabledSources);
  }

  if (pooledData.empty()) {
    result.errorMessage = "No entropy data available";
    return result;
  }

  const int format = g_state.outputFormat;
  // Every format except One-Time Pad generates a list of items
  const bool isList = (format != 6);
  const size_t quantity =
      isList ? static_cast<size_t>(std::max(g_state.outputQuantity, 1)) : 1;

  // Calculate required bytes per item based on output format
  // IMPORTANT: Must match what the format generators actually consume!
  size_t bytesPerItem = 0;

  switch (format) {
  case 0: // Decimal - uses 2 bytes per digit. Request 4x for rejection safety
          // buffer.
    bytesPerItem = static_cast<size_t>(g_state.decimalDigits) * 4;
    break;
  case 1: // Integer - one 8-byte draw; raised below for rejection retries
    bytesPerItem = 8;
    break;
  case 3: // Custom - uses 2 bytes per character. Request 4x for rejection
          // safety buffer.
    bytesPerItem = static_cast<size_t>(g_state.customLength) * 4;
    break;
  case 4: { // Raw Data - what the chosen encoding consumes (see GetRawLayout)
    const uint64_t bits =
        (g_state.bitByteUnit == 0)
            ? static_cast<uint64_t>(g_state.bitByteAmount)
            : static_cast<uint64_t>(g_state.bitByteAmount) * 8;
    bytesPerItem = static_cast<size_t>(
        GetRawLayout(g_state.bitByteFormat, bits).bytes);
    break;
  }
  case 5: // Passphrase - 3 bytes per word
    bytesPerItem = static_cast<size_t>(g_state.passphraseWordCount) * 3;
    break;
  case 6: // OTP - 1 byte per input (Text Mode needs 2x for Rejection)
    if (g_state.otpInputMode == 0) {
      // Text mode: ASCII safe Modulo-95.
      // We request 2x bytes to ensure Rejection Sampling < 190 succeeds.
      bytesPerItem = strlen(g_state.otpMessage.data()) * 2;
    } else {
      // File mode: Raw XOR, 1:1 ratio
      bytesPerItem = static_cast<size_t>(g_state.otpFileSize);
    }
    break;
  case 7: // UUIDv4 - 16 bytes per UUID (version/variant bits overwritten)
    bytesPerItem = 16;
    break;
  case 8: // UUIDv7 - 10 bytes random field + 6 bytes counter seed/increment
    bytesPerItem = 16;
    break;
  }

  // Rejection-sampling formats keep the same retry headroom per item
  // whatever the quantity (e.g. 8 draws per integer)
  if ((format == 0 || format == 1 || format == 3) &&
      bytesPerItem < AppConfig::REJECTION_MIN_BYTES_PER_ITEM)
    bytesPerItem = AppConfig::REJECTION_MIN_BYTES_PER_ITEM;

  if (bytesPerItem > 0 && quantity > SIZE_MAX / bytesPerItem) {
    result.errorMessage =
        "Requested output is too large. Reduce the quantity or item size.";
    Crypto::SecureClearVector(pooledData);
    return result;
  }
  size_t bytesNeeded = bytesPerItem * quantity;

  // Ensure minimum security (512 bits = 64 bytes minimum)
  if (bytesNeeded < 64)
    bytesNeeded = 64;

  Logger::Log(Logger::Level::INFO, "CSPRNG", "Mode: %s",
              mode == GenerationMode::Consolidation
                  ? "CONSOLIDATION (TRUE RANDOMNESS) - collected entropy met "
                    "the target"
                  : "EXPANSION (PSEUDO-RANDOM) - collected entropy below the "
                    "target");

  // Generate random bytes
  std::vector<uint8_t> randomBytes;
  std::vector<uint8_t> itemBytes; // One item's slice of randomBytes
  auto wipeAll = [&]() {
    Crypto::SecureClearVector(randomBytes);
    Crypto::SecureClearVector(itemBytes);
    Crypto::SecureClearVector(pooledData);
    Crypto::SecureClearVector(result.output);
  };
  try {
      randomBytes = GenerateRandomBytes(pooledData, bytesNeeded);
  result.rawBytesGenerated = randomBytes.size();

  if (format == 6) { // OTP
    if (g_state.otpInputMode == 0) {
      // Text input
      result.output =
          GenerateOTP(randomBytes, std::string(g_state.otpMessage.data()));
    } else {
      // File input - read file and XOR
      std::ifstream file(g_state.otpFilePath.data(), std::ios::binary);
      if (!file.is_open()) {
        result.errorMessage = "Failed to open input file";
        wipeAll();
        return result;
      }

      std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
      file.close();

      std::vector<uint8_t> encrypted = GenerateOTPFile(randomBytes, fileData);

      // Output as hex
      std::ostringstream oss;
      for (uint8_t b : encrypted) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
      }
      std::string s = oss.str();
      result.output = std::vector<char>(s.begin(), s.end());
      SecureZeroMemory(s.data(), s.size());

      // Secure cleanup
      SecureZeroMemory(fileData.data(), fileData.size());
      SecureZeroMemory(encrypted.data(), encrypted.size());
    }
  } else {
    // Load wordlist if needed
    if (format == 5 && !LoadWordListForGeneration()) {
      result.errorMessage = "Failed to load wordlist";
      wipeAll();
      return result;
    }
    const std::string separator =
        (format == 5) ? std::string(g_state.passphraseSeparator.data())
                      : std::string();

    UUIDv7State v7State;  // Keeps UUIDv7s of this batch in creation order
    UUIDv7Options v7Options;
    v7Options.method = g_state.uuidV7Method;
    v7Options.counterBits = g_state.uuidV7CounterBits;
    v7Options.incrementBits = g_state.uuidV7IncrementBits;
    uint32_t attempt = 0; // Unique per retry so each gets fresh bytes

    for (size_t i = 0; i < quantity; i++) {
      if ((i & 0x3FF) == 0 && g_state.cancelGeneration) throw std::runtime_error("Generation cancelled by user.");

      // Each item gets its own, non-overlapping slice of the random bytes
      itemBytes.assign(randomBytes.begin() + i * bytesPerItem,
                       randomBytes.begin() + (i + 1) * bytesPerItem);
      std::vector<char> item;

      switch (format) {
      case 0: // Decimal
        item = GenerateDecimal(itemBytes, g_state.decimalDigits);
        break;

      case 1: { // Integer
        bool exhausted = false;
        item = GenerateInteger(itemBytes, g_state.integerMin,
                               g_state.integerMax, exhausted);

        // Every draw rejected (only plausible for ranges just over 2^63, ~1
        // in 256): retry with fresh bytes rather than use a biased value.
        for (unsigned retry = 1;
             exhausted && retry <= AppConfig::INTEGER_MAX_RETRIES; retry++) {
          ++attempt;
          Logger::Log(Logger::Level::INFO, "CSPRNG",
                      "Integer item %zu: all draws rejected, retrying "
                      "(attempt %u)",
                      i + 1, attempt);
          Crypto::SecureClearVector(itemBytes);
          itemBytes = GenerateRandomBytes(pooledData, bytesPerItem, attempt);
          item = GenerateInteger(itemBytes, g_state.integerMin,
                                 g_state.integerMax, exhausted);
        }

        if (exhausted) {
          result.errorMessage = "Integer generation failed: every random "
                                "draw was rejected after retries. Please try "
                                "again.";
          wipeAll();
          return result;
        }
        break;
      }

      case 3: // Custom
        item = GenerateCustomString(itemBytes, g_state.customLength,
                                    g_state.includeNumbers,
                                    g_state.includeUppercase,
                                    g_state.includeLowercase,
                                    g_state.includeSpecial);
        break;

      case 4: { // Raw Data
        const int enc = g_state.bitByteFormat;
        const bool grouped = enc >= 0 && enc < RAW_ENCODING_COUNT &&
                             g_state.rawGroupEnabled[enc];
        item = GenerateBitByte(itemBytes, g_state.bitByteAmount,
                               g_state.bitByteUnit, enc,
                               grouped ? g_state.rawGroupSize[enc] : 0,
                               enc >= 0 && enc < RAW_ENCODING_COUNT &&
                                   g_state.rawUppercase[enc]);
        break;
      }

      case 5: // Passphrase
        item = GeneratePassphrase(itemBytes, g_state.passphraseWordCount,
                                  separator, g_state.cachedWordList);
        break;

      case 7: // UUIDv4
        item = GenerateUUIDv4(itemBytes, g_state.uuidUppercase,
                              g_state.uuidHyphens);
        break;

      case 8: // UUIDv7
        item = GenerateUUIDv7(itemBytes, g_state.uuidUppercase,
                              g_state.uuidHyphens, v7Options, v7State);
        break;
      }

      AppendListItem(result.output, i, quantity, item, g_state.listNumbered,
                     g_state.listBlankLine);
      Crypto::SecureClearVector(item);
    }
  }

  // Ensure null termination for C-string compatibility
  ReserveSecure(result.output, 1);
  result.output.push_back('\0');

  // Calculate entropy consumed
  result.entropyConsumed = static_cast<float>(randomBytes.size()) * 8.0f;
  if (mode == GenerationMode::Consolidation) {
    // In consolidation, we consumed what we hashed
    result.entropyConsumed =
        std::min(result.entropyConsumed, g_state.collectedBits);
  }

  // Secure cleanup
  Crypto::SecureClearVector(randomBytes);
  Crypto::SecureClearVector(itemBytes);
  Crypto::SecureClearVector(pooledData);

  result.success = true;
  Logger::Log(Logger::Level::INFO, "CSPRNG",
              "Output generated successfully. Format: %d, Items: %zu, Mode: "
              "%s, Bytes: %zu",
              format, quantity,
              mode == GenerationMode::Consolidation ? "CONSOLIDATION"
                                                    : "EXPANSION",
              result.rawBytesGenerated);

  } catch (const std::bad_alloc&) {
      wipeAll();
      result.errorMessage = "Memory allocation failed! The requested amount is too large for the system's available RAM/Paging file. Reduce the generation size bounds.";
      result.success = false;
      Logger::Log(Logger::Level::ERR, "CSPRNG", "std::bad_alloc caught! Generation request exceeded available memory.");
  } catch (const std::exception& e) {
      wipeAll();
      result.errorMessage = std::string("Unexpected generation error: ") + e.what();
      result.success = false;
      Logger::Log(Logger::Level::ERR, "CSPRNG", "std::exception caught during generation.");
  }

  return result;
}

void GenerateNistData(const std::string &filepath, size_t totalBytes) {
  g_state.isExportingNist = true;
  g_state.nistBytesWritten = 0;
  g_state.nistTotalBytes = totalBytes;
  g_state.nistError = "";

  std::ofstream file(filepath, std::ios::binary);
  if (!file.is_open()) {
    g_state.nistError = "Failed to open output file";
    g_state.isExportingNist = false;
    return;
  }

  // Capture initial entropy state
  std::set<Entropy::EntropySource> sources;
  // Enable all available sources for the seed
  sources.insert(Entropy::EntropySource::ClockDrift);
  sources.insert(Entropy::EntropySource::CpuJitter);
  // (User input sources might be empty if automated, but we use what we have)

  std::vector<Entropy::EntropyDataPoint> pooledData =
      g_state.entropyPool.GetPooledData();

  if (pooledData.empty()) {
    // Fallback: Force some collection if completely empty?
    // For now, let GenerateRandomBytes handle it (it will just be deterministic
    // if empty) But in a real scenario we'd want to block.
    Logger::Log(Logger::Level::WARN, "CSPRNG",
                "Exporting NIST data with empty entropy pool!");
  }

  const size_t CHUNK_SIZE = 1024 * 1024; // 1 MB chunks
  size_t remaining = totalBytes;
  std::vector<uint8_t> buffer;
  buffer.reserve(CHUNK_SIZE);

  while (remaining > 0 &&
         g_state.isExportingNist) { // Check flag to allow cancellation
    size_t currentChunk = std::min(remaining, CHUNK_SIZE);

    // Use Quad-Layer Generator (same as main output)
    // Note: In a real NIST test, we want to test the DRBG mechanism mostly.
    // We reuse the pooledData for the seed.
    // Ideally we would "reseed" occasionally, but for 100MB expansion from one
    // seed is fine for testing the expansion algo. Actually, to simulate
    // continuous operation, we should refresh the pool if possible, but this
    // function runs in a tight loop. The background threads might be adding
    // entropy. validData = g_state.entropyPool.GetPooledData(); <-- would need
    // mutex

    // For this test: We use the *initial* seed and let the CSPRNG expand it.
    // This validates the specific CSPRNG "Expansion" algorithm (HKDF ->
    // ChaCha20 -> AES -> ChaCha20).

    // We pass 'pooledData' (the seed) every time?
    // GenerateRandomBytes uses the seed to derive keys.
    // If we pass the SAME seed every time, we get the SAME output?
    // WAIT. GenerateRandomBytes is stateless!
    // It deterministically derives keys from the entropy pool!
    // If the pool doesn't change, the keys don't change!
    // AND 'GenerateStream' uses a nonce, but does it increment?
    // 'GenerateRandomBytes' creates a fresh ChaCha20 key/nonce from HKDF(seed).
    // If seed is identical, output is identical!

    // FIX: we need to vary the input to GenerateRandomBytes or modify it to be
    // stateful. Since we can't easily make it stateful without refactoring, we
    // will manually inject a counter into the "entropy" for this export loop.

    // Hack: Add a synthetic counter data point to the pool copy for each chunk
    // This ensures unique key derivation for each 1MB chunk.
    pooledData.push_back({
        Entropy::GetNanosecondTimestamp(), // Increasing timestamp
        static_cast<uint64_t>(
            g_state.nistBytesWritten.load()), // Increasing counter (value)
        Entropy::EntropySource::CpuJitter     // Fake source
    });

    // Generate chunk
    std::vector<uint8_t> chunk =
        GenerateRandomBytes(pooledData, currentChunk);

    // Write to file
    file.write(reinterpret_cast<const char *>(chunk.data()), chunk.size());

    // Update progress
    g_state.nistBytesWritten += currentChunk;
    remaining -= currentChunk;

    // Cleanup chunk
    SecureZeroMemory(chunk.data(), chunk.size());
  }

  file.close();

  // Cleanup pool copy
  SecureZeroMemory(pooledData.data(),
                   pooledData.size() * sizeof(Entropy::EntropyDataPoint));

  g_state.isExportingNist = false;
  Logger::Log(Logger::Level::INFO, "CSPRNG",
              "NIST Data Export verify complete: %zu bytes", totalBytes);
}

} // namespace CSPRNG
