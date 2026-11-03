#include "csprng.h"
#include "../crypto/secure_mem.h"
#include "formats/cancellation.h"
#include "../logging/logger.h"
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
    Logger::Log(Logger::Level::WARN, "CSPRNG",
                "Windows random generator unavailable, seeding without it");
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

void SeedGenerator(Entropy::GenerationInput &input, uint64_t generationNumber) {
  Crypto::SecureBytes seedMaterial = BuildPointBytes(input.newPoints);
  if (!input.generator) {
    AppendOsRandom(seedMaterial);
    const Crypto::SecureBytes nonce = BuildNonce(generationNumber);
    const Crypto::ByteSpan personalization{
        reinterpret_cast<const uint8_t *>(PERSONALIZATION_STRING),
        sizeof(PERSONALIZATION_STRING) - 1};
    input.generator.Emplace(SpanOf(seedMaterial), SpanOf(nonce), personalization);
  } else if (!seedMaterial.empty()) {
    AppendOsRandom(seedMaterial);
    input.generator->Reseed(SpanOf(seedMaterial), {});
  }
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

static void ReseedDuringGeneration(Crypto::HmacDrbg &generator,
                                   const ReseedSource &reseedSource) {
  Entropy::EntropyDataVector points =
      reseedSource.pool.TakeAdditionalInput(reseedSource.input, reseedSource.includedSources);
  Crypto::SecureBytes seedMaterial = BuildPointBytes(points);
  Crypto::SecureClearVector(points);
  AppendOsRandom(seedMaterial);
  if (!seedMaterial.empty())
    generator.Reseed(SpanOf(seedMaterial), {});
}

std::vector<uint8_t> GenerateRandomBytes(Crypto::HmacDrbg &generator, size_t byteCount,
                                         Crypto::ByteSpan additionalInput,
                                         const ReseedSource *reseedSource) {
  std::vector<uint8_t> bytes(byteCount);
  try {
    size_t written = 0;
    size_t bytesSinceReseed = 0;
    while (written < byteCount) {
      ThrowIfGenerationCancelled();
      if (reseedSource && bytesSinceReseed >= RESEED_INTERVAL_BYTES) {
        ReseedDuringGeneration(generator, *reseedSource);
        bytesSinceReseed = 0;
      }
      const size_t requestBytes =
          std::min(byteCount - written, Crypto::HmacDrbg::MAX_BYTES_PER_REQUEST);
      if (!generator.Generate(bytes.data() + written, requestBytes, additionalInput))
        throw std::runtime_error("HMAC_DRBG refused the request: reseed required.");
      additionalInput = {};
      written += requestBytes;
      bytesSinceReseed += requestBytes;
    }
  } catch (...) {
    Crypto::SecureClearVector(bytes);
    throw;
  }
  return bytes;
}

}
