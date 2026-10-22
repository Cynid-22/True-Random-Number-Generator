#include "csprng.h"
#include "../core/app_state.h"
#include "../crypto/quad_layer.h"
#include "../crypto/sha512.h"
#include "../logging/logger.h"
#include <chrono>
#include <sstream>
#include <string>
#include <windows.h>

namespace CSPRNG {

static const char CARRY_OVER_LABEL[] = "TRNG-CARRY-OVER";
static constexpr size_t BYTES_PER_DATA_POINT = 16;

Crypto::SecureBytes BuildSeedMaterial(const Entropy::GenerationInput &input) {
  Crypto::SecureBytes bytes;
  bytes.reserve((input.hasCarryOver ? input.carryOver.size() : 0) +
                input.newPoints.size() * BYTES_PER_DATA_POINT);

  if (input.hasCarryOver)
    bytes.insert(bytes.end(), input.carryOver.begin(), input.carryOver.end());

  for (const auto &point : input.newPoints) {
    for (int i = 0; i < 8; i++) {
      bytes.push_back(static_cast<uint8_t>(point.timestamp >> (i * 8)));
    }
    for (int i = 0; i < 8; i++) {
      bytes.push_back(static_cast<uint8_t>(point.value >> (i * 8)));
    }
  }

  return bytes;
}

Entropy::CarryOver DeriveCarryOver(const Crypto::SecureBytes &seedMaterial) {
  Crypto::SHA512::Hash seedHash =
      Crypto::SHA512::Compute(seedMaterial.data(), seedMaterial.size());
  const Entropy::CarryOver carryOver = Crypto::SHA512::HMAC(
      seedHash.data(), seedHash.size(),
      reinterpret_cast<const uint8_t *>(CARRY_OVER_LABEL),
      sizeof(CARRY_OVER_LABEL) - 1);
  SecureZeroMemory(seedHash.data(), seedHash.size());
  return carryOver;
}

static int64_t WallClockNanoseconds() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(system_clock::now().time_since_epoch())
      .count();
}

static std::string BuildLayerOneInfo(const OutputSettings &settings,
                                     size_t byteCount, uint64_t generationNumber,
                                     uint32_t attempt) {
  std::ostringstream info;
  info << "TRNG-L1|Len:" << byteCount << "|Fmt:" << settings.outputFormat << "|";
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
  if (attempt > 0)
    info << "|R:" << attempt;
  return info.str();
}

struct ScopedCryptoPassFlag {
  ScopedCryptoPassFlag() { g_state.generationInCryptoPass = true; }
  ~ScopedCryptoPassFlag() { g_state.generationInCryptoPass = false; }
};

std::vector<uint8_t> GenerateRandomBytes(const OutputSettings &settings,
                                         const Crypto::SecureBytes &seedMaterial,
                                         uint64_t generationNumber,
                                         size_t byteCount, uint32_t attempt) {
  ScopedCryptoPassFlag cryptoPassFlag;

  Logger::Log(Logger::Level::INFO, "CSPRNG",
              "GenerateRandomBytes: %zu seed bytes, requesting %zu output "
              "bytes (generation %llu, attempt %u)",
              seedMaterial.size(), byteCount,
              static_cast<unsigned long long>(generationNumber), attempt);

  const std::string infoText =
      BuildLayerOneInfo(settings, byteCount, generationNumber, attempt);
  const std::vector<uint8_t> info(infoText.begin(), infoText.end());

  return Crypto::GenerateQuadLayerStream(seedMaterial.data(), seedMaterial.size(),
                                         info, byteCount);
}

}
