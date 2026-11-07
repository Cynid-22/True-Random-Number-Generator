#include "csprng.h"
#include "../crypto/secure_mem.h"
#include "formats/cancellation.h"
#include "../platform/os_random.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <windows.h>

namespace CSPRNG {

static const char PERSONALIZATION_STRING[] = "TRNG HMAC_DRBG";
static constexpr size_t BYTES_PER_DATA_POINT = 16;
static constexpr size_t OS_RANDOM_BYTES = 64;
static constexpr size_t RESEED_INTERVAL_BYTES = 64 * 1024 * 1024;

template <typename Integer>
static void AppendLittleEndian(Crypto::SecureBytes &bytes, Integer value) {
  for (size_t i = 0; i < sizeof(Integer); i++) {
    bytes.push_back(static_cast<uint8_t>(static_cast<uint64_t>(value) >> (i * 8)));
  }
}

static void AppendOsRandom(Crypto::SecureBytes &bytes) {
  const size_t start = bytes.size();
  bytes.resize(start + OS_RANDOM_BYTES);
  if (!Platform::FillOsRandom(bytes.data() + start, OS_RANDOM_BYTES)) {
    bytes.resize(start);
  }
}

static Crypto::SecureBytes BuildPointBytes(const Entropy::EntropyDataVector &points) {
  Crypto::SecureBytes bytes;
  bytes.reserve(points.size() * BYTES_PER_DATA_POINT + OS_RANDOM_BYTES);
  for (const auto &point : points) {
    AppendLittleEndian(bytes, point.timestamp);
    AppendLittleEndian(bytes, point.value);
  }
  return bytes;
}

static int64_t WallClockNanoseconds() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(system_clock::now().time_since_epoch())
      .count();
}

static Crypto::SecureBytes BuildNonce(uint64_t generationNumber) {
  Crypto::SecureBytes nonce;
  AppendLittleEndian(nonce, WallClockNanoseconds());
  AppendLittleEndian(nonce, generationNumber);
  AppendLittleEndian(nonce, static_cast<uint32_t>(GetCurrentProcessId()));
  return nonce;
}

static Crypto::ByteSpan SpanOf(const Crypto::SecureBytes &bytes) {
  return {bytes.data(), bytes.size()};
}

std::string BuildGenerationInfo(const OutputSettings &settings, size_t byteCount,
                                uint64_t generationNumber) {
  std::ostringstream info;
  info << "TRNG|Len:" << byteCount << "|Fmt:" << settings.outputFormat << "|";
  switch (settings.outputFormat) {
  case FORMAT_DECIMAL:
    info << "D:" << settings.decimalDigits;
    break;
  case FORMAT_INTEGER:
    info << "I:" << settings.integerMin << ":" << settings.integerMax;
    break;
  case FORMAT_RANDOM_CHARACTERS:
    info << "C:" << settings.randomCharactersLength;
    break;
  case FORMAT_RAW_DATA:
    info << "U:" << settings.rawDataUnit << "A:" << settings.rawDataAmount
         << "E:" << settings.rawDataEncoding;
    break;
  case FORMAT_PASSPHRASE:
    info << "W:" << settings.passphraseWordCount
         << "L:" << settings.passphraseWordList
         << "N:" << settings.passphraseNumberCount;
    break;
  case FORMAT_ONE_TIME_PAD:
    info << "O:" << settings.oneTimePadInputMode;
    if (settings.oneTimePadInputMode == ONE_TIME_PAD_MESSAGE_LENGTH_INPUT)
      info << "L:" << settings.oneTimePadMessageLength;
    break;
  }
  if (settings.outputFormat != FORMAT_ONE_TIME_PAD)
    info << "|Q:" << settings.outputQuantity;
  info << "|T:" << WallClockNanoseconds();
  info << "|G:" << generationNumber;
  return info.str();
}

static void ReseedDuringGeneration(Entropy::GenerationInput &input,
                                   const ReseedSource &reseedSource) {
  Entropy::EntropyDataVector points =
      reseedSource.pool.TakeAdditionalInput(input, reseedSource.includedSources);
  Crypto::SecureBytes seedMaterial = BuildPointBytes(points);
  Crypto::SecureClearVector(points);
  AppendOsRandom(seedMaterial);
  if (!seedMaterial.empty())
    input.generator->Reseed(SpanOf(seedMaterial), {});
}

static uint8_t *TakeReserveBytes(Entropy::GenerationInput &input, size_t byteCount) {
  if (input.reserveBytes.size() - input.reserveBytesUsed < byteCount)
    throw ReserveRanOut();
  uint8_t *bytes = input.reserveBytes.data() + input.reserveBytesUsed;
  input.reserveBytesUsed += byteCount;
  return bytes;
}

static void XorReserveInto(Entropy::GenerationInput &input, uint8_t *output,
                           size_t byteCount) {
  uint8_t *reserveBytes = TakeReserveBytes(input, byteCount);
  for (size_t i = 0; i < byteCount; i++) {
    output[i] ^= reserveBytes[i];
  }
  SecureZeroMemory(reserveBytes, byteCount);
}

bool GeneratorNeedsReserveSeed(const Entropy::GenerationInput &input) {
  return !input.generator && input.CreditBits() < AppConfig::MINIMUM_ENTROPY_BITS;
}

void SeedGenerator(Entropy::GenerationInput &input, uint64_t generationNumber) {
  Crypto::SecureBytes seedMaterial = BuildPointBytes(input.newPoints);
  if (!input.generator) {
    if (GeneratorNeedsReserveSeed(input)) {
      uint8_t *reserveBytes = TakeReserveBytes(input, GENERATOR_RESERVE_SEED_BYTES);
      seedMaterial.insert(seedMaterial.end(), reserveBytes,
                          reserveBytes + GENERATOR_RESERVE_SEED_BYTES);
      SecureZeroMemory(reserveBytes, GENERATOR_RESERVE_SEED_BYTES);
      input.reserveBitsSeedingGenerator = AppConfig::MINIMUM_ENTROPY_BITS;
    }
    AppendOsRandom(seedMaterial);
    const Crypto::SecureBytes nonce = BuildNonce(generationNumber);
    const Crypto::ByteSpan personalization{
        reinterpret_cast<const uint8_t *>(PERSONALIZATION_STRING),
        sizeof(PERSONALIZATION_STRING) - 1};
    input.generator.Emplace(SpanOf(seedMaterial), SpanOf(nonce), personalization);
    input.generatorBytesSinceFullReseed = 0;
  } else if (!seedMaterial.empty()) {
    AppendOsRandom(seedMaterial);
    input.generator->Reseed(SpanOf(seedMaterial), {});
  }
}

static void ReseedFromReserve(Entropy::GenerationInput &input) {
  uint8_t *reserveBytes = TakeReserveBytes(input, TRUE_PATH_RESEED_BYTES);
  Crypto::SecureBytes entropyInput(reserveBytes, reserveBytes + TRUE_PATH_RESEED_BYTES);
  SecureZeroMemory(reserveBytes, TRUE_PATH_RESEED_BYTES);
  AppendOsRandom(entropyInput);
  input.generator->Reseed(SpanOf(entropyInput), {});
  input.generatorBytesSinceFullReseed = 0;
}

size_t TruePathReserveBytes(size_t outputBytes, uint64_t generatorBytesSinceFullReseed) {
  const uint64_t bytesBeforeFirstReseed =
      generatorBytesSinceFullReseed >= TRUE_PATH_RESEED_INTERVAL_BYTES
          ? 0
          : TRUE_PATH_RESEED_INTERVAL_BYTES - generatorBytesSinceFullReseed;
  if (outputBytes <= bytesBeforeFirstReseed)
    return outputBytes;

  const uint64_t bytesAfterFirstReseed = outputBytes - bytesBeforeFirstReseed;
  const uint64_t reseedCount =
      bytesAfterFirstReseed / TRUE_PATH_RESEED_INTERVAL_BYTES +
      (bytesAfterFirstReseed % TRUE_PATH_RESEED_INTERVAL_BYTES != 0 ? 1 : 0);
  const uint64_t reseedBytes = reseedCount * TRUE_PATH_RESEED_BYTES;
  if (outputBytes > SIZE_MAX - reseedBytes)
    return SIZE_MAX;
  return outputBytes + static_cast<size_t>(reseedBytes);
}

const char *ModeName(GenerationMode mode) {
  return mode == GenerationMode::TrueRandom ? "TRUE RANDOM" : "PSEUDO-RANDOM";
}

std::vector<uint8_t> GenerateRandomBytes(Entropy::GenerationInput &input, size_t byteCount,
                                         GenerationMode mode,
                                         Crypto::ByteSpan additionalInput,
                                         const ReseedSource *reseedSource) {
  const bool xorWithReserve = mode == GenerationMode::TrueRandom;
  std::vector<uint8_t> bytes(byteCount);
  try {
    size_t written = 0;
    size_t bytesSinceReseed = 0;
    while (written < byteCount) {
      ThrowIfGenerationCancelled();
      if (reseedSource && bytesSinceReseed >= RESEED_INTERVAL_BYTES) {
        ReseedDuringGeneration(input, *reseedSource);
        bytesSinceReseed = 0;
      }
      size_t requestBytes =
          std::min(byteCount - written, Crypto::HmacDrbg::MAX_BYTES_PER_REQUEST);
      if (xorWithReserve) {
        if (input.generatorBytesSinceFullReseed >= TRUE_PATH_RESEED_INTERVAL_BYTES)
          ReseedFromReserve(input);
        requestBytes = std::min(requestBytes,
                                static_cast<size_t>(TRUE_PATH_RESEED_INTERVAL_BYTES -
                                                    input.generatorBytesSinceFullReseed));
      }
      if (!input.generator->Generate(bytes.data() + written, requestBytes, additionalInput))
        throw std::runtime_error("HMAC_DRBG refused the request: reseed required.");
      if (xorWithReserve)
        XorReserveInto(input, bytes.data() + written, requestBytes);
      additionalInput = {};
      written += requestBytes;
      bytesSinceReseed += requestBytes;
      input.generatorBytesSinceFullReseed += requestBytes;
    }
  } catch (...) {
    Crypto::SecureClearVector(bytes);
    throw;
  }
  return bytes;
}

}
