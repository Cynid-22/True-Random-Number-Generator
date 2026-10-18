#include "output_generator.h"
#include "../../config/AppConfig.h"
#include "../core/app_state.h"
#include "../crypto/secure_mem.h"
#include "../logging/logger.h"
#include "csprng.h"
#include "formats/cancellation.h"
#include "formats/number_formats.h"
#include "formats/one_time_pad_format.h"
#include "formats/raw_data_format.h"
#include "formats/text_formats.h"
#include "formats/uuid_formats.h"
#include "logic.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <new>
#include <sstream>
#include <windows.h>

namespace CSPRNG {

static void ReserveWithoutLeavingCopies(std::vector<char> &output,
                                        size_t extraBytes) {
  size_t needed = output.size() + extraBytes;
  if (needed <= output.capacity())
    return;
  std::vector<char> larger;
  larger.reserve(std::max(needed, output.capacity() * 2));
  larger.insert(larger.end(), output.begin(), output.end());
  Crypto::SecureClearVector(output);
  output.swap(larger);
}

static void AppendListItem(std::vector<char> &output, size_t index,
                           size_t quantity, const std::vector<char> &item,
                           bool numbered, bool blankLine) {
  std::string prefix = numbered ? std::to_string(index + 1) + ". " : "";
  size_t separatorLength = (index == 0) ? 0 : (blankLine ? 2 : 1);

  if (index == 0) {
    size_t estimatedItemSize =
        item.size() + 2 + (numbered ? std::to_string(quantity).size() + 2 : 0);
    if (quantity <= (SIZE_MAX - 1) / estimatedItemSize)
      ReserveWithoutLeavingCopies(output, estimatedItemSize * quantity + 1);
  }

  ReserveWithoutLeavingCopies(output, separatorLength + prefix.size() + item.size());
  if (index > 0) {
    output.push_back('\n');
    if (blankLine)
      output.push_back('\n');
  }
  output.insert(output.end(), prefix.begin(), prefix.end());
  output.insert(output.end(), item.begin(), item.end());
}

static size_t CalculateBytesPerItem(int format) {
  size_t bytesPerItem = 0;

  switch (format) {
  case FORMAT_DECIMAL:
    bytesPerItem = static_cast<size_t>(g_state.decimalDigits) * 4;
    break;
  case FORMAT_INTEGER:
    bytesPerItem = 8;
    break;
  case FORMAT_RANDOM_CHARACTERS:
    bytesPerItem = static_cast<size_t>(g_state.randomCharactersLength) * 4;
    break;
  case FORMAT_RAW_DATA:
    bytesPerItem = static_cast<size_t>(
        GetRawDataLayout(g_state.rawDataEncoding,
                         RawDataBitCount(g_state.rawDataAmount,
                                         g_state.rawDataUnit))
            .randomByteCount);
    break;
  case FORMAT_PASSPHRASE:
    bytesPerItem = static_cast<size_t>(g_state.passphraseWordCount) * 3;
    break;
  case FORMAT_ONE_TIME_PAD:
    if (g_state.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT) {
      bytesPerItem = strlen(g_state.oneTimePadMessage.data()) * 2;
    } else {
      bytesPerItem = static_cast<size_t>(g_state.oneTimePadFileSize);
    }
    break;
  case FORMAT_UUID_V4:
  case FORMAT_UUID_V7:
    bytesPerItem = 16;
    break;
  }

  if ((format == FORMAT_DECIMAL || format == FORMAT_INTEGER ||
       format == FORMAT_RANDOM_CHARACTERS) &&
      bytesPerItem < AppConfig::REJECTION_MIN_BYTES_PER_ITEM)
    bytesPerItem = AppConfig::REJECTION_MIN_BYTES_PER_ITEM;

  return bytesPerItem;
}

static bool WriteOneTimePadOutput(const std::vector<uint8_t> &randomBytes,
                                  GenerationResult &result) {
  if (g_state.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT) {
    result.output = GenerateOneTimePadText(
        randomBytes, std::string(g_state.oneTimePadMessage.data()));
    return true;
  }

  std::ifstream file(g_state.oneTimePadFilePath.data(), std::ios::binary);
  if (!file.is_open()) {
    result.errorMessage = "Failed to open input file";
    return false;
  }

  std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
  file.close();

  std::vector<uint8_t> encrypted = GenerateOneTimePadFile(randomBytes, fileData);

  std::ostringstream hexStream;
  for (uint8_t byte : encrypted) {
    hexStream << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(byte);
  }
  std::string hexText = hexStream.str();
  result.output = std::vector<char>(hexText.begin(), hexText.end());
  SecureZeroMemory(hexText.data(), hexText.size());

  SecureZeroMemory(fileData.data(), fileData.size());
  SecureZeroMemory(encrypted.data(), encrypted.size());
  return true;
}

static bool WriteListOutput(int format, size_t quantity, size_t bytesPerItem,
                            const std::vector<uint8_t> &randomBytes,
                            std::vector<uint8_t> &itemBytes,
                            const std::vector<Entropy::EntropyDataPoint> &pooledData,
                            GenerationResult &result) {
  if (format == FORMAT_PASSPHRASE && !LoadWordListForGeneration()) {
    result.errorMessage = "Failed to load wordlist";
    return false;
  }
  const std::string separator =
      (format == FORMAT_PASSPHRASE)
          ? std::string(g_state.passphraseSeparator.data())
          : std::string();

  UUIDv7State uuidV7State;
  UUIDv7Options uuidV7Options;
  uuidV7Options.ordering = g_state.uuidV7Ordering;
  uuidV7Options.counterBits = g_state.uuidV7CounterBits;
  uuidV7Options.incrementBits = g_state.uuidV7IncrementBits;
  uint32_t attempt = 0;

  for (size_t i = 0; i < quantity; i++) {
    if ((i & 0x3FF) == 0)
      ThrowIfGenerationCancelled();

    itemBytes.assign(randomBytes.begin() + i * bytesPerItem,
                     randomBytes.begin() + (i + 1) * bytesPerItem);
    std::vector<char> item;

    switch (format) {
    case FORMAT_DECIMAL:
      item = GenerateDecimal(itemBytes, g_state.decimalDigits);
      break;

    case FORMAT_INTEGER: {
      bool allDrawsRejected = false;
      item = GenerateInteger(itemBytes, g_state.integerMin,
                             g_state.integerMax, allDrawsRejected);

      for (unsigned retry = 1;
           allDrawsRejected && retry <= AppConfig::INTEGER_MAX_RETRIES; retry++) {
        ++attempt;
        Logger::Log(Logger::Level::INFO, "CSPRNG",
                    "Integer item %zu: all draws rejected, retrying "
                    "(attempt %u)",
                    i + 1, attempt);
        Crypto::SecureClearVector(itemBytes);
        itemBytes = GenerateRandomBytes(pooledData, bytesPerItem, attempt);
        item = GenerateInteger(itemBytes, g_state.integerMin,
                               g_state.integerMax, allDrawsRejected);
      }

      if (allDrawsRejected) {
        result.errorMessage = "Integer generation failed: every random "
                              "draw was rejected after retries. Please try "
                              "again.";
        return false;
      }
      break;
    }

    case FORMAT_RANDOM_CHARACTERS:
      item = GenerateRandomCharacters(itemBytes, g_state.randomCharactersLength,
                                      g_state.includeNumbers,
                                      g_state.includeUppercase,
                                      g_state.includeLowercase,
                                      g_state.includeSpecial);
      break;

    case FORMAT_RAW_DATA: {
      const int encoding = g_state.rawDataEncoding;
      const bool validEncoding = encoding >= 0 && encoding < RAW_ENCODING_COUNT;
      const bool grouped = validEncoding && g_state.rawDataGroupingEnabled[encoding];
      item = GenerateRawData(itemBytes, g_state.rawDataAmount,
                             g_state.rawDataUnit, encoding,
                             grouped ? g_state.rawDataGroupSize[encoding] : 0,
                             validEncoding && g_state.rawDataUppercase[encoding]);
      break;
    }

    case FORMAT_PASSPHRASE:
      item = GeneratePassphrase(itemBytes, g_state.passphraseWordCount,
                                separator, g_state.cachedWordList);
      break;

    case FORMAT_UUID_V4:
      item = GenerateUUIDv4(itemBytes, g_state.uuidUppercase,
                            g_state.uuidHyphens);
      break;

    case FORMAT_UUID_V7:
      item = GenerateUUIDv7(itemBytes, g_state.uuidUppercase,
                            g_state.uuidHyphens, uuidV7Options, uuidV7State);
      break;
    }

    AppendListItem(result.output, i, quantity, item, g_state.numberListItems,
                   g_state.blankLineBetweenItems);
    Crypto::SecureClearVector(item);
  }
  return true;
}

GenerationResult GenerateOutput(GenerationMode mode) {
  GenerationResult result;
  result.success = false;
  result.mode = mode;
  result.entropyConsumed = 0.0f;
  result.rawBytesGenerated = 0;

  std::vector<Entropy::EntropyDataPoint> pooledData =
      g_state.entropyPool.GetPooledData();

  if (pooledData.empty()) {
    result.errorMessage = "No entropy data available";
    return result;
  }

  const int format = g_state.outputFormat;
  const bool isList = (format != FORMAT_ONE_TIME_PAD);
  const size_t quantity =
      isList ? static_cast<size_t>(std::max(g_state.outputQuantity, 1)) : 1;

  const size_t bytesPerItem = CalculateBytesPerItem(format);

  if (bytesPerItem > 0 && quantity > SIZE_MAX / bytesPerItem) {
    result.errorMessage =
        "Requested output is too large. Reduce the quantity or item size.";
    Crypto::SecureClearVector(pooledData);
    return result;
  }
  size_t bytesNeeded = bytesPerItem * quantity;

  if (bytesNeeded < 64)
    bytesNeeded = 64;

  Logger::Log(Logger::Level::INFO, "CSPRNG", "Mode: %s",
              mode == GenerationMode::Consolidation
                  ? "CONSOLIDATION (TRUE RANDOMNESS) - collected entropy met "
                    "the target"
                  : "EXPANSION (PSEUDO-RANDOM) - collected entropy below the "
                    "target");

  std::vector<uint8_t> randomBytes;
  std::vector<uint8_t> itemBytes;
  auto wipeAll = [&]() {
    Crypto::SecureClearVector(randomBytes);
    Crypto::SecureClearVector(itemBytes);
    Crypto::SecureClearVector(pooledData);
    Crypto::SecureClearVector(result.output);
  };
  try {
    randomBytes = GenerateRandomBytes(pooledData, bytesNeeded);
    result.rawBytesGenerated = randomBytes.size();

    const bool written =
        isList ? WriteListOutput(format, quantity, bytesPerItem, randomBytes,
                                 itemBytes, pooledData, result)
               : WriteOneTimePadOutput(randomBytes, result);
    if (!written) {
      wipeAll();
      return result;
    }

    ReserveWithoutLeavingCopies(result.output, 1);
    result.output.push_back('\0');

    result.entropyConsumed = static_cast<float>(randomBytes.size()) * 8.0f;
    if (mode == GenerationMode::Consolidation) {
      result.entropyConsumed =
          std::min(result.entropyConsumed, g_state.collectedEntropyBits);
    }

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

  } catch (const std::bad_alloc &) {
    wipeAll();
    result.errorMessage = "Memory allocation failed! The requested amount is too large for the system's available RAM/Paging file. Reduce the generation size bounds.";
    result.success = false;
    Logger::Log(Logger::Level::ERR, "CSPRNG", "std::bad_alloc caught! Generation request exceeded available memory.");
  } catch (const std::exception &e) {
    wipeAll();
    result.errorMessage = std::string("Unexpected generation error: ") + e.what();
    result.success = false;
    Logger::Log(Logger::Level::ERR, "CSPRNG", "std::exception caught during generation.");
  }

  return result;
}

}
