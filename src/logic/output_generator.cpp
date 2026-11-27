#include "output_generator.h"
#include "../../config/AppConfig.h"
#include "../core/app_state.h"
#include "../crypto/secure_mem.h"
#include "../platform/unicode_path.h"
#include "csprng.h"
#include "formats/cancellation.h"
#include "formats/number_formats.h"
#include "formats/one_time_pad_format.h"
#include "formats/raw_data_format.h"
#include "formats/text_formats.h"
#include "formats/uuid_formats.h"
#include "list_input.h"
#include "word_lists.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <new>
#include <numeric>
#include <windows.h>

namespace CSPRNG {

static std::atomic<uint64_t> s_generationCount{0};
static constexpr size_t LIST_INDEX_BYTES = 4;
static constexpr size_t LIST_CANCEL_CHECK_MASK = 0x3FF;

static void AppendListItem(std::vector<char> &output, size_t index,
                           size_t quantity, const char *item, size_t itemLength,
                           bool numbered, bool blankLine) {
  std::string prefix = numbered ? std::to_string(index + 1) + ". " : "";
  size_t separatorLength = (index == 0) ? 0 : (blankLine ? 2 : 1);

  if (index == 0) {
    size_t estimatedItemSize =
        itemLength + 2 + (numbered ? std::to_string(quantity).size() + 2 : 0);
    if (quantity <= (SIZE_MAX - 1) / estimatedItemSize)
      Crypto::ReserveWithoutLeavingCopies(output,
                                          estimatedItemSize * quantity + 1);
  }

  Crypto::ReserveWithoutLeavingCopies(
      output, separatorLength + prefix.size() + itemLength);
  if (index > 0) {
    output.push_back('\n');
    if (blankLine)
      output.push_back('\n');
  }
  output.insert(output.end(), prefix.begin(), prefix.end());
  output.insert(output.end(), item, item + itemLength);
}

static void AppendItemIndex(std::vector<uint8_t> &binaryOutput, size_t index) {
  Crypto::ReserveWithoutLeavingCopies(binaryOutput, LIST_INDEX_BYTES);
  for (size_t b = 0; b < LIST_INDEX_BYTES; b++)
    binaryOutput.push_back(static_cast<uint8_t>(index >> (8 * b)));
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
  case FORMAT_LIST_PICK:
  case FORMAT_LIST_SHUFFLE:
    bytesPerItem = ListRandomByteCount(settings);
    break;
  }

  const bool usesRejectionSampling =
      format == FORMAT_DECIMAL || format == FORMAT_INTEGER ||
      format == FORMAT_RANDOM_CHARACTERS ||
      (format == FORMAT_ONE_TIME_PAD &&
       settings.oneTimePadInputMode != ONE_TIME_PAD_FILE_INPUT);
  if (usesRejectionSampling && bytesPerItem < AppConfig::REJECTION_MIN_BYTES_PER_ITEM)
    bytesPerItem = AppConfig::REJECTION_MIN_BYTES_PER_ITEM;

  return bytesPerItem;
}

static size_t OutputQuantity(const OutputSettings &settings) {
  if (settings.outputFormat == FORMAT_ONE_TIME_PAD || IsListFormat(settings.outputFormat))
    return 1;
  return static_cast<size_t>(std::max(settings.outputQuantity, 1));
}

size_t RandomByteCount(const OutputSettings &settings) {
  const size_t bytesPerItem = CalculateBytesPerItem(settings, settings.outputFormat);
  const size_t quantity = OutputQuantity(settings);
  if (bytesPerItem > 0 && quantity > SIZE_MAX / bytesPerItem)
    return SIZE_MAX;
  return bytesPerItem * quantity;
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
                            Entropy::GenerationInput &input, GenerationMode mode,
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
        Crypto::SecureClearVector(itemBytes);
        itemBytes = GenerateRandomBytes(input, bytesPerItem, mode);
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

    AppendListItem(result.output, i, quantity, item.data(), item.size(),
                   settings.numberListItems, settings.blankLineBetweenItems);
    AppendBinaryItem(result.binaryOutput, i, quantity, itemBinary);
    Crypto::SecureClearVector(item);
    Crypto::SecureClearVector(itemBinary);
  }
  return true;
}

static bool WritePickedItemsOutput(const OutputSettings &settings,
                                   const std::vector<uint8_t> &randomBytes,
                                   GenerationResult &result) {
  const std::string_view text = ListTextOf(settings);
  const std::vector<ListField> fields = SplitList(text, ListFormatOf(settings));
  const std::vector<size_t> itemFields = ListItemFieldIndices(fields);
  const std::vector<uint64_t> weights = ListPickWeights(settings, text, fields, itemFields);
  const size_t quantity = static_cast<size_t>(std::max(settings.outputQuantity, 1));

  std::vector<size_t> picks =
      PickListItems(randomBytes, weights, quantity, ListPicksCanRepeat(settings));
  std::vector<uint8_t> removeField;
  auto wipePicks = [&]() {
    Crypto::SecureClearVector(picks);
    Crypto::SecureClearVector(removeField);
  };
  try {
    if (picks.size() <= SIZE_MAX / LIST_INDEX_BYTES)
      Crypto::ReserveWithoutLeavingCopies(result.binaryOutput, picks.size() * LIST_INDEX_BYTES);
    for (size_t i = 0; i < picks.size(); i++) {
      if ((i & LIST_CANCEL_CHECK_MASK) == 0)
        ThrowIfGenerationCancelled();
      const std::string_view item = ListItemText(text, fields[itemFields[picks[i]]]);
      AppendListItem(result.output, i, picks.size(), item.data(), item.size(),
                     settings.numberListItems, settings.blankLineBetweenItems);
      AppendItemIndex(result.binaryOutput, picks[i]);
    }
    if (settings.listRemovePicked) {
      removeField.assign(fields.size(), 0);
      for (size_t pick : picks)
        removeField[itemFields[pick]] = 1;
      result.updatedListText = RemoveListFields(text, fields, removeField);
      result.updatesListText = true;
      result.removedListItemCount = picks.size();
    }
  } catch (...) {
    wipePicks();
    throw;
  }
  wipePicks();
  return true;
}

static bool WriteShuffledListOutput(const OutputSettings &settings,
                                    const std::vector<uint8_t> &randomBytes,
                                    GenerationResult &result) {
  const std::string_view text = ListTextOf(settings);
  const ListFormat format = ListFormatOf(settings);
  const std::vector<ListField> fields = SplitList(text, format);
  const std::vector<size_t> itemFields = ListItemFieldIndices(fields);

  std::vector<size_t> itemOrder(itemFields.size());
  std::vector<size_t> fieldOrder;
  auto wipeOrder = [&]() {
    Crypto::SecureClearVector(itemOrder);
    Crypto::SecureClearVector(fieldOrder);
  };
  try {
    std::iota(itemOrder.begin(), itemOrder.end(), size_t(0));
    ShuffleListOrder(randomBytes, itemOrder);
    fieldOrder.reserve(itemOrder.size());
    for (size_t itemIndex : itemOrder)
      fieldOrder.push_back(itemFields[itemIndex]);
    const std::string_view joiner = ListJoiner(format);

    if (settings.shuffleListInPlace) {
      result.updatedListText = JoinListItems(text, fields, fieldOrder, joiner);
      result.updatesListText = true;
    } else if (ListJoinsWithLineBreaks(format)) {
      for (size_t i = 0; i < fieldOrder.size(); i++) {
        if ((i & LIST_CANCEL_CHECK_MASK) == 0)
          ThrowIfGenerationCancelled();
        const std::string_view item = ListItemText(text, fields[fieldOrder[i]]);
        AppendListItem(result.output, i, fieldOrder.size(), item.data(), item.size(),
                       settings.numberListItems, settings.blankLineBetweenItems);
      }
    } else {
      result.output = JoinListItems(text, fields, fieldOrder, joiner);
      result.joinedItemLengths.reserve(fieldOrder.size());
      for (size_t fieldIndex : fieldOrder)
        result.joinedItemLengths.push_back(fields[fieldIndex].itemLength);
      result.joinedSeparatorLength = joiner.size();
    }

    if (!settings.shuffleListInPlace) {
      Crypto::ReserveWithoutLeavingCopies(result.binaryOutput,
                                          itemOrder.size() * LIST_INDEX_BYTES);
      for (size_t itemIndex : itemOrder)
        AppendItemIndex(result.binaryOutput, itemIndex);
    }
  } catch (...) {
    wipeOrder();
    throw;
  }
  wipeOrder();
  return true;
}

static bool WriteOutput(const OutputSettings &settings, int format, size_t quantity,
                        size_t bytesPerItem, const std::vector<uint8_t> &randomBytes,
                        std::vector<uint8_t> &itemBytes, Entropy::GenerationInput &input,
                        GenerationMode mode, GenerationResult &result) {
  switch (format) {
  case FORMAT_ONE_TIME_PAD:
    return WriteOneTimePadOutput(settings, randomBytes, result);
  case FORMAT_LIST_PICK:
    return WritePickedItemsOutput(settings, randomBytes, result);
  case FORMAT_LIST_SHUFFLE:
    return WriteShuffledListOutput(settings, randomBytes, result);
  default:
    return WriteListOutput(settings, format, quantity, bytesPerItem, randomBytes, itemBytes,
                           input, mode, result);
  }
}

GenerationResult GenerateOutput(
    const std::set<Entropy::EntropySource> &includedSources,
    const OutputSettings &settings) {
  GenerationResult result;
  result.success = false;
  result.cancelled = false;
  result.mode = GenerationMode::PseudoRandom;
  result.oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;

  if (g_state.selfTestFailed) {
    result.errorMessage = "Generating is turned off: the cryptographic "
                          "self-test failed at launch.";
    return result;
  }

  const int format = settings.outputFormat;
  const bool isList = (format != FORMAT_ONE_TIME_PAD);
  const size_t quantity = OutputQuantity(settings);

  if (IsListFormat(format)) {
    result.errorMessage = ListInputError(settings);
    if (!result.errorMessage.empty())
      return result;
  }

  const size_t bytesPerItem = CalculateBytesPerItem(settings, format);

  if (bytesPerItem > 0 && quantity > SIZE_MAX / bytesPerItem) {
    result.errorMessage =
        "Requested output is too large. Reduce the quantity or item size.";
    return result;
  }
  const size_t bytesNeeded = bytesPerItem * quantity;
  const size_t integerRetryBytes = (format == FORMAT_INTEGER) ? bytesNeeded : 0;
  const size_t outputReserveBytes = TruePathReserveBytes(
      bytesNeeded > SIZE_MAX - integerRetryBytes ? SIZE_MAX : bytesNeeded + integerRetryBytes,
      TRUE_PATH_RESEED_INTERVAL_BYTES);
  const size_t reserveBytesToCopy =
      outputReserveBytes > SIZE_MAX - GENERATOR_RESERVE_SEED_BYTES
          ? SIZE_MAX
          : outputReserveBytes + GENERATOR_RESERVE_SEED_BYTES;

  const uint64_t generationNumber = ++s_generationCount;
  Entropy::GenerationInput input;
  std::vector<uint8_t> randomBytes;
  std::vector<uint8_t> itemBytes;
  auto wipeOutput = [&]() {
    Crypto::SecureClearVector(randomBytes);
    Crypto::SecureClearVector(itemBytes);
    Crypto::SecureClearVector(result.output);
    Crypto::SecureClearVector(result.binaryOutput);
    Crypto::SecureClearVector(result.oneTimePadKey);
    Crypto::SecureClearVector(result.oneTimePadCiphertext);
    result.oneTimePadResultMode = ONE_TIME_PAD_NO_RESULT;
    Crypto::SecureClearVector(result.updatedListText);
    result.updatesListText = false;
    result.removedListItemCount = 0;
    result.joinedItemLengths.clear();
    result.joinedSeparatorLength = 0;
  };
  auto wipeAll = [&]() {
    input.SecureWipe();
    wipeOutput();
  };
  try {
    input = g_state.entropyPool.CopyGenerationInput(includedSources, reserveBytesToCopy);

    const bool generatorSeedTooShort =
        input.previouslyUsedBits + input.CreditBits() < AppConfig::MINIMUM_ENTROPY_BITS;
    if (generatorSeedTooShort &&
        (!GeneratorNeedsReserveSeed(input) ||
         input.reserveBytes.size() < GENERATOR_RESERVE_SEED_BYTES)) {
      wipeAll();
      result.errorMessage =
          "Collect at least " +
          std::to_string(static_cast<int>(AppConfig::MINIMUM_ENTROPY_BITS)) +
          " bits of entropy before generating.";
      return result;
    }

    SeedGenerator(input, generationNumber);
    GenerationMode mode =
        input.reserveBytes.size() - input.reserveBytesUsed >=
                TruePathReserveBytes(bytesNeeded, input.generatorBytesSinceFullReseed)
            ? GenerationMode::TrueRandom
            : GenerationMode::PseudoRandom;
    if (!isList && mode != GenerationMode::TrueRandom) {
      wipeAll();
      result.errorMessage =
          "One-Time Pad requires TRUE RANDOM mode: the true random reserve "
          "doesn't cover the pad. Collect more entropy.";
      return result;
    }

    const std::string generationInfo =
        BuildGenerationInfo(settings, bytesNeeded, generationNumber);
    const ReseedSource reseedSource{g_state.entropyPool, includedSources};
    auto generateAndWrite = [&](GenerationMode outputMode) {
      randomBytes = GenerateRandomBytes(
          input, bytesNeeded, outputMode,
          {reinterpret_cast<const uint8_t *>(generationInfo.data()),
           generationInfo.size()},
          &reseedSource);
      ThrowIfGenerationCancelled();
      return WriteOutput(settings, format, quantity, bytesPerItem, randomBytes, itemBytes,
                         input, outputMode, result);
    };

    bool written = false;
    try {
      written = generateAndWrite(mode);
    } catch (const ReserveRanOut &) {
      if (!isList)
        throw;
      wipeOutput();
      mode = GenerationMode::PseudoRandom;
      written = generateAndWrite(mode);
    }
    if (!written) {
      wipeAll();
      return result;
    }
    result.mode = mode;

    if (result.oneTimePadResultMode == ONE_TIME_PAD_NO_RESULT) {
      Crypto::ReserveWithoutLeavingCopies(result.output, 1);
      result.output.push_back('\0');
    }

    g_state.entropyPool.UseGenerationInput(input);

    input.SecureWipe();
    Crypto::SecureClearVector(randomBytes);
    Crypto::SecureClearVector(itemBytes);

    result.success = true;
  } catch (const GenerationCancelled &cancelled) {
    wipeAll();
    result.errorMessage = cancelled.what();
    result.cancelled = true;
    result.success = false;
  } catch (const std::bad_alloc &) {
    wipeAll();
    result.errorMessage = "Memory allocation failed! The requested amount is too large for the system's available RAM/Paging file. Reduce the generation size bounds.";
    result.success = false;
  } catch (const std::exception &e) {
    wipeAll();
    result.errorMessage = std::string("Unexpected generation error: ") + e.what();
    result.success = false;
  }

  return result;
}

}
