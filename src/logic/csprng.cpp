#include "csprng.h"
#include "../core/app_state.h"
#include "../crypto/quad_layer.h"
#include "../logging/logger.h"
#include <sstream>
#include <string>
#include <windows.h>

namespace CSPRNG {

static std::vector<uint8_t>
SerializeEntropyData(const std::vector<Entropy::EntropyDataPoint> &data) {
  std::vector<uint8_t> bytes;
  bytes.reserve(data.size() * 16);

  for (const auto &point : data) {
    for (int i = 0; i < 8; i++) {
      bytes.push_back(static_cast<uint8_t>(point.timestamp >> (i * 8)));
    }
    for (int i = 0; i < 8; i++) {
      bytes.push_back(static_cast<uint8_t>(point.value >> (i * 8)));
    }
  }

  return bytes;
}

static std::string BuildLayerOneInfo(size_t byteCount, uint32_t attempt) {
  std::ostringstream info;
  info << "TRNG-L1|Len:" << byteCount << "|Fmt:" << g_state.outputFormat << "|";
  switch (g_state.outputFormat) {
  case FORMAT_DECIMAL:
    info << "D:" << g_state.decimalDigits;
    break;
  case FORMAT_INTEGER:
    info << "I:" << g_state.integerMin << ":" << g_state.integerMax;
    break;
  case FORMAT_RANDOM_CHARACTERS:
    info << "C:" << g_state.randomCharactersLength;
    break;
  case FORMAT_RAW_DATA:
    info << "U:" << g_state.rawDataUnit << "A:" << g_state.rawDataAmount
         << "E:" << g_state.rawDataEncoding;
    break;
  case FORMAT_PASSPHRASE:
    info << "W:" << g_state.passphraseWordCount
         << "L:" << g_state.passphraseWordList
         << "N:" << g_state.passphraseNumberCount;
    break;
  case FORMAT_ONE_TIME_PAD:
    info << "O:" << g_state.oneTimePadInputMode;
    if (g_state.oneTimePadInputMode == ONE_TIME_PAD_MESSAGE_LENGTH_INPUT)
      info << "L:" << g_state.oneTimePadMessageLength;
    break;
  }
  if (g_state.outputFormat != FORMAT_ONE_TIME_PAD)
    info << "|Q:" << g_state.outputQuantity;
  info << "|T:" << Entropy::GetNanosecondTimestamp();
  if (attempt > 0)
    info << "|R:" << attempt;
  return info.str();
}

struct ScopedCryptoPassFlag {
  ScopedCryptoPassFlag() { g_state.generationInCryptoPass = true; }
  ~ScopedCryptoPassFlag() { g_state.generationInCryptoPass = false; }
};

std::vector<uint8_t>
GenerateRandomBytes(const std::vector<Entropy::EntropyDataPoint> &entropyData,
                    size_t byteCount, uint32_t attempt) {
  ScopedCryptoPassFlag cryptoPassFlag;
  std::vector<uint8_t> entropyBytes = SerializeEntropyData(entropyData);

  Logger::Log(Logger::Level::INFO, "CSPRNG",
              "GenerateRandomBytes: %zu data points, %zu entropy bytes, "
              "requesting %zu output bytes (attempt %u)",
              entropyData.size(), entropyBytes.size(), byteCount, attempt);

  const std::string infoText = BuildLayerOneInfo(byteCount, attempt);
  const std::vector<uint8_t> info(infoText.begin(), infoText.end());

  std::vector<uint8_t> result =
      Crypto::GenerateQuadLayerStream(entropyBytes, info, byteCount);

  SecureZeroMemory(entropyBytes.data(), entropyBytes.size());

  return result;
}

}
