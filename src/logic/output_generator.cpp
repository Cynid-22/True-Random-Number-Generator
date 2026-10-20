#include "output_generator.h"
#include "../../config/AppConfig.h"
#include "../core/app_state.h"
#include "../crypto/secure_mem.h"
#include "../logging/logger.h"
#include "../platform/unicode_path.h"
#include "csprng.h"
#include "formats/cancellation.h"
#include "formats/number_formats.h"
#include "formats/one_time_pad_format.h"
#include "formats/raw_data_format.h"
#include "formats/text_formats.h"
#include "formats/uuid_formats.h"
#include "word_lists.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>
#include <windows.h>

namespace CSPRNG {

static std::atomic<uint64_t> s_generationCount{0};

static void AppendListItem(std::vector<char> &output, size_t index,
                           size_t quantity, const std::vector<char> &item,
                           bool numbered, bool blankLine) {
  std::string prefix = numbered ? std::to_string(index + 1) + ". " : "";
  size_t separatorLength = (index == 0) ? 0 : (blankLine ? 2 : 1);

  if (index == 0) {
    size_t estimatedItemSize =
        item.size() + 2 + (numbered ? std::to_string(quantity).size() + 2 : 0);
    if (quantity <= (SIZE_MAX - 1) / estimatedItemSize)
      Crypto::ReserveWithoutLeavingCopies(output,
                                          estimatedItemSize * quantity + 1);
  }

  Crypto::ReserveWithoutLeavingCopies(
      output, separatorLength + prefix.size() + item.size());
  if (index > 0) {
    output.push_back('\n');
    if (blankLine)
      output.push_back('\n');
  }
  output.insert(output.end(), prefix.begin(), prefix.end());
  output.insert(output.end(), item.begin(), item.end());
}

static void AppendBinaryItem(std::vector<uint8_t> &binaryOutput, size_t index,
                             size_t quantity,
                             const std::vector<uint8_t> &itemBinary) {
  if (index == 0 && !itemBinary.empty() &&
      quantity <= SIZE_MAX / itemBinary.size())
    Crypto::ReserveWithoutLeavingCopies(binaryOutput,
                                        itemBinary.size() * quantity);

  Crypto::ReserveWithoutLeavingCopies(binaryOutput, itemBinary.size());
  binaryOutput.insert(binaryOutput.end(), itemBinary.begin(), itemBinary.end());
}

static size_t CalculateBytesPerItem(const OutputSettings &settings, int format) {
  size_t bytesPerItem = 0;

  switch (format) {
  case FORMAT_DECIMAL:
    bytesPerItem = static_cast<size_t>(settings.decimalDigits) * 4;
    break;
  case FORMAT_INTEGER:
    bytesPerItem = 8;
    break;
  case FORMAT_RANDOM_CHARACTERS:
    bytesPerItem = static_cast<size_t>(settings.randomCharactersLength) * 4;
    break;
  case FORMAT_RAW_DATA:
    bytesPerItem = static_cast<size_t>(
        GetRawDataLayout(settings.rawDataEncoding,
                         RawDataBitCount(settings.rawDataAmount,
                                         settings.rawDataUnit))
            .randomByteCount);
    break;
  case FORMAT_PASSPHRASE: {
    const size_t DRAWS_PER_NUMBER = 2;
    const size_t drawCount =
        static_cast<size_t>(settings.passphraseWordCount) +
        static_cast<size_t>(settings.passphraseNumberCount) * DRAWS_PER_NUMBER;
    const size_t drawBytes = drawCount * 3;
    bytesPerItem = drawBytes + drawBytes / 4 +
                   AppConfig::REJECTION_MIN_BYTES_PER_ITEM;
    break;
  }
  case FORMAT_ONE_TIME_PAD:
    if (settings.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT) {
      bytesPerItem = strlen(settings.oneTimePadMessage.data()) * 2;
    } else if (settings.oneTimePadInputMode ==
               ONE_TIME_PAD_MESSAGE_LENGTH_INPUT) {
      bytesPerItem = static_cast<size_t>(settings.oneTimePadMessageLength) * 2;
    } else {
      bytesPerItem = static_cast<size_t>(settings.oneTimePadFileSize);
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

static const char *FileNameOfPath(const char *path) {
  const char *name = path;
  for (const char *c = path; *c != '\0'; c++) {
    if (*c == '\\' || *c == '/')
      name = c + 1;
  }
  return name;
}

static bool WriteOneTimePadFileOutput(const OutputSettings &settings,
                                      const std::vector<uint8_t> &randomBytes,
                                      GenerationResult &result) {
  const char *path = settings.oneTimePadFilePath.data();
  std::vector<uint8_t> fileData;
  const Platform::FileStamp selectedStamp{settings.oneTimePadFileSize,
                                          settings.oneTimePadFileLastWriteTime};
  switch (Platform::ReadWholeFileIfUnchanged(path, selectedStamp, fileData)) {
  case Platform::FileReadStatus::Ok:
    break;
  case Platform::FileReadStatus::Changed:
    result.errorMessage = ONE_TIME_PAD_FILE_CHANGED_ERROR;
    return false;
  case Platform::FileReadStatus::OpenFailed:
    result.errorMessage = "Failed to open the input file.";
    return false;
  case Platform::FileReadStatus::ReadFailed:
    result.errorMessage = "Failed to read the input file.";
    return false;
  }

  bool encrypted = false;
  try {
    encrypted = GenerateOneTimePadFile(randomBytes, fileData,
                                       result.oneTimePadKey,
                                       result.oneTimePadCiphertext);
  } catch (...) {
    Crypto::SecureClearVector(fileData);
    throw;
  }
  Crypto::SecureClearVector(fileData);
  if (!encrypted) {
    result.errorMessage = "Not enough random data for the file. Please retry.";
    return false;
  }

  result.oneTimePadResultMode = ONE_TIME_PAD_FILE_INPUT;
  result.oneTimePadFileName = FileNameOfPath(path);
  return true;
}

static bool WriteOneTimePadOutput(const OutputSettings &settings,
                                  const std::vector<uint8_t> &randomBytes,
                                  GenerationResult &result) {
  if (settings.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT) {
    const char *message = settings.oneTimePadMessage.data();
    if (!GenerateOneTimePadText(randomBytes, message, strlen(message),
                                result.oneTimePadKey,
                                result.oneTimePadCiphertext,
                                result.errorMessage))
      return false;
    result.oneTimePadResultMode = ONE_TIME_PAD_TEXT_INPUT;
    return true;
  }

  if (settings.oneTimePadInputMode == ONE_TIME_PAD_MESSAGE_LENGTH_INPUT) {
    if (!GenerateOneTimePadKey(
            randomBytes,
            static_cast<size_t>(settings.oneTimePadMessageLength),
            result.output)) {
      result.errorMessage = "Insufficient entropy (rejection sampling "
                            "exhausted). Please retry.";
      return false;
    }
    result.binaryOutput =
        OneTimePadValues(result.output.data(), result.output.size());
    return true;
  }

  return WriteOneTimePadFileOutput(settings, randomBytes, result);
}

static bool WriteListOutput(const OutputSettings &settings, int format,
                            size_t quantity, size_t bytesPerItem,
                            const std::vector<uint8_t> &randomBytes,
                            std::vector<uint8_t> &itemBytes,
                            const Crypto::SecureBytes &seedMaterial,
                            uint64_t generationNumber,
                            GenerationResult &result) {
  const WordLists::WordList &wordList =
      WordLists::Get(settings.passphraseWordList);
  if (format == FORMAT_PASSPHRASE && !wordList.IsUsable()) {
    result.errorMessage = wordList.loadError;
    return false;
  }
  const std::string separator =
      (format == FORMAT_PASSPHRASE)
          ? std::string(settings.passphraseSeparator.data())
          : std::string();

  UUIDv7State uuidV7State;
  UUIDv7Options uuidV7Options;
  uuidV7Options.ordering = settings.uuidV7Ordering;
  uuidV7Options.counterBits = settings.uuidV7CounterBits;
  uuidV7Options.incrementBits = settings.uuidV7IncrementBits;
  uint32_t attempt = 0;

  for (size_t i = 0; i < quantity; i++) {
    if ((i & 0x3FF) == 0)
      ThrowIfGenerationCancelled();

    itemBytes.assign(randomBytes.begin() + i * bytesPerItem,
                     randomBytes.begin() + (i + 1) * bytesPerItem);
    std::vector<char> item;
    std::vector<uint8_t> itemBinary;

    switch (format) {
    case FORMAT_DECIMAL:
      item = GenerateDecimal(itemBytes, settings.decimalDigits, itemBinary);
      break;

    case FORMAT_INTEGER: {
      bool allDrawsRejected = false;
      item = GenerateInteger(itemBytes, settings.integerMin,
                             settings.integerMax, allDrawsRejected, itemBinary);

      for (unsigned retry = 1;
           allDrawsRejected && retry <= AppConfig::INTEGER_MAX_RETRIES; retry++) {
        ++attempt;
        Logger::Log(Logger::Level::INFO, "CSPRNG",
                    "Integer item %zu: all draws rejected, retrying "
                    "(attempt %u)",
                    i + 1, attempt);
        Crypto::SecureClearVector(itemBytes);
        itemBytes = GenerateRandomBytes(settings, seedMaterial, generationNumber,
                                        bytesPerItem, attempt);
        item = GenerateInteger(itemBytes, settings.integerMin,
                               settings.integerMax, allDrawsRejected,
                               itemBinary);
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
      item = GenerateRandomCharacters(itemBytes, settings.randomCharactersLength,
                                      settings.includeNumbers,
                                      settings.includeUppercase,
                                      settings.includeLowercase,
                                      settings.includeSpecial, itemBinary);
      break;

    case FORMAT_RAW_DATA: {
      const int encoding = settings.rawDataEncoding;
      const bool validEncoding = encoding >= 0 && encoding < RAW_ENCODING_COUNT;
      const bool grouped = validEncoding && settings.rawDataGroupingEnabled[encoding];
      item = GenerateRawData(itemBytes, settings.rawDataAmount,
                             settings.rawDataUnit, encoding,
                             grouped ? settings.rawDataGroupSize[encoding] : 0,
                             validEncoding && settings.rawDataUppercase[encoding],
                             itemBinary);
      break;
    }

    case FORMAT_PASSPHRASE:
      item = GeneratePassphrase(itemBytes, settings.passphraseWordCount,
                                settings.passphraseNumberCount, separator,
                                wordList.words, itemBinary);
      break;

    case FORMAT_UUID_V4:
      item = GenerateUUIDv4(itemBytes, settings.uuidUppercase,
                            settings.uuidHyphens, itemBinary);
      break;

    case FORMAT_UUID_V7:
      item = GenerateUUIDv7(itemBytes, settings.uuidUppercase,
                            settings.uuidHyphens, uuidV7Options, uuidV7State,
                            itemBinary);
      break;
    }

    AppendListItem(result.output, i, quantity, item, settings.numberListItems,
                   settings.blankLineBetweenItems);
    AppendBinaryItem(result.binaryOutput, i, quantity, itemBinary);
    Crypto::SecureClearVector(item);
    Crypto::SecureClearVector(itemBinary);
  }
  return true;
}

GenerationResult GenerateOutput(
    GenerationMode mode,
    const std::set<Entropy::EntropySource> &includedSources,
    const OutputSettings &settings) {
  GenerationResult result;
  result.success = false;
  result.cancelled = false;
  result.mode = mode;
  result.entropyConsumed = 0.0f;
  result.rawBytesGenerated = 0;
  result.oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;

  const int format = settings.outputFormat;
  const bool isList = (format != FORMAT_ONE_TIME_PAD);
  const size_t quantity =
      isList ? static_cast<size_t>(std::max(settings.outputQuantity, 1)) : 1;

  const size_t bytesPerItem = CalculateBytesPerItem(settings, format);

  if (bytesPerItem > 0 && quantity > SIZE_MAX / bytesPerItem) {
    result.errorMessage =
        "Requested output is too large. Reduce the quantity or item size.";
    return result;
  }
  size_t bytesNeeded = bytesPerItem * quantity;

  if (bytesNeeded < 64)
    bytesNeeded = 64;

  const uint64_t CHACHA20_MAX_KEYSTREAM_BYTES = 64ULL << 32;
  if (static_cast<uint64_t>(bytesNeeded) > CHACHA20_MAX_KEYSTREAM_BYTES) {
    result.errorMessage = "Requested output is too large: it needs more than "
                          "256 GiB of random data. Reduce the quantity or "
                          "item size.";
    return result;
  }

  const uint64_t generationNumber = ++s_generationCount;
  Entropy::GenerationInput input;
  Entropy::CarryOver nextCarryOver{};
  Crypto::SecureBytes seedMaterial;
  std::vector<uint8_t> randomBytes;
  std::vector<uint8_t> itemBytes;
  auto wipeAll = [&]() {
    input.SecureWipe();
    SecureZeroMemory(nextCarryOver.data(), nextCarryOver.size());
    Crypto::SecureClearVector(seedMaterial);
    Crypto::SecureClearVector(randomBytes);
    Crypto::SecureClearVector(itemBytes);
    Crypto::SecureClearVector(result.output);
    Crypto::SecureClearVector(result.binaryOutput);
    Crypto::SecureClearVector(result.oneTimePadKey);
    Crypto::SecureClearVector(result.oneTimePadCiphertext);
    result.oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;
  };
  try {
    input = g_state.entropyPool.CopyGenerationInput(includedSources);
    Logger::Log(Logger::Level::INFO, "CSPRNG",
                "Generation %llu: %zu new data points, %.1f bits credited, "
                "carry-over %s",
                static_cast<unsigned long long>(generationNumber),
                input.newPoints.size(), input.CreditBits(),
                input.hasCarryOver ? "included" : "none");

    if (input.newPoints.empty() && !input.hasCarryOver) {
      wipeAll();
      result.errorMessage = "No entropy data available";
      return result;
    }

    Logger::Log(Logger::Level::INFO, "CSPRNG", "Mode: %s",
                mode == GenerationMode::EntropyBacked
                    ? "ENTROPY-BACKED - unused entropy met the target"
                    : "EXPANDED - unused entropy below the target");

    seedMaterial = BuildSeedMaterial(input);
    randomBytes =
        GenerateRandomBytes(settings, seedMaterial, generationNumber, bytesNeeded);
    result.rawBytesGenerated = randomBytes.size();
    ThrowIfGenerationCancelled();

    const bool written =
        isList ? WriteListOutput(settings, format, quantity, bytesPerItem, randomBytes,
                                 itemBytes, seedMaterial, generationNumber,
                                 result)
               : WriteOneTimePadOutput(settings, randomBytes, result);
    if (!written) {
      wipeAll();
      return result;
    }

    if (result.oneTimePadResultMode == ONE_TIME_PAD_NO_RESULT) {
      Crypto::ReserveWithoutLeavingCopies(result.output, 1);
      result.output.push_back('\0');
    }

    nextCarryOver = DeriveCarryOver(seedMaterial);
    g_state.entropyPool.UseGenerationInput(input, nextCarryOver);
    result.entropyConsumed = static_cast<float>(input.CreditBits());

    input.SecureWipe();
    SecureZeroMemory(nextCarryOver.data(), nextCarryOver.size());
    Crypto::SecureClearVector(seedMaterial);
    Crypto::SecureClearVector(randomBytes);
    Crypto::SecureClearVector(itemBytes);

    result.success = true;
    Logger::Log(Logger::Level::INFO, "CSPRNG",
                "Output generated successfully. Format: %d, Items: %zu, Mode: "
                "%s, Bytes: %zu",
                format, quantity,
                mode == GenerationMode::EntropyBacked ? "ENTROPY-BACKED"
                                                      : "EXPANDED",
                result.rawBytesGenerated);

  } catch (const GenerationCancelled &cancelled) {
    wipeAll();
    result.errorMessage = cancelled.what();
    result.cancelled = true;
    result.success = false;
    Logger::Log(Logger::Level::INFO, "CSPRNG", "Generation cancelled by user.");
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
