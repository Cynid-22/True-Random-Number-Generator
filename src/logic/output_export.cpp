#include "output_export.h"
#include "formats/raw_data_format.h"
#include <cstdint>
#include <cstring>

static constexpr char CSV_LINE_END[] = "\r\n";
static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";

static void Append(std::vector<char> &output, const char *data,
                   size_t length) {
  Crypto::ReserveWithoutLeavingCopies(output, length);
  output.insert(output.end(), data, data + length);
}

static void Append(std::vector<char> &output, const char *text) {
  Append(output, text, strlen(text));
}

static void Append(std::vector<char> &output, const std::string &text) {
  Append(output, text.data(), text.size());
}

static OutputSetting NumberSetting(const char *name, long long value) {
  return {name, std::to_string(value), false};
}

static OutputSetting BooleanSetting(const char *name, bool value) {
  return {name, value ? "true" : "false", false};
}

static OutputSetting TextSetting(const char *name, const std::string &value) {
  return {name, value, true};
}

static const char *UuidV7OrderingName(int ordering) {
  switch (ordering) {
  case UUIDV7_DEDICATED_COUNTER:
    return "Method 1: Dedicated counter";
  case UUIDV7_MONOTONIC_RANDOM:
    return "Method 2: Monotonic random";
  default:
    return "None: Fully random";
  }
}

static const char *OneTimePadInputName(int inputMode) {
  switch (inputMode) {
  case ONE_TIME_PAD_TEXT_INPUT:
    return "Text Input";
  case ONE_TIME_PAD_MESSAGE_LENGTH_INPUT:
    return "Message Length";
  default:
    return "File Input";
  }
}

static void AddRawDataSettings(std::vector<OutputSetting> &settings) {
  settings.push_back(NumberSetting("amount", g_state.rawDataAmount));
  settings.push_back(TextSetting(
      "unit", g_state.rawDataUnit == RAW_DATA_UNIT_BITS ? "bits" : "bytes"));

  const int encoding = g_state.rawDataEncoding;
  if (encoding < 0 || encoding >= CSPRNG::RAW_ENCODING_COUNT)
    return;
  const bool grouped = g_state.rawDataGroupingEnabled[encoding];
  settings.push_back(BooleanSetting("grouping", grouped));
  if (grouped)
    settings.push_back(
        NumberSetting("groupSize", g_state.rawDataGroupSize[encoding]));
  if (CSPRNG::RawEncodingHasSingleCaseLetters(encoding))
    settings.push_back(
        BooleanSetting("uppercase", g_state.rawDataUppercase[encoding]));
}

static void AddUuidSettings(std::vector<OutputSetting> &settings) {
  settings.push_back(BooleanSetting("uppercase", g_state.uuidUppercase));
  settings.push_back(BooleanSetting("hyphens", g_state.uuidHyphens));
  if (g_state.outputFormat != FORMAT_UUID_V7)
    return;

  settings.push_back(
      TextSetting("ordering", UuidV7OrderingName(g_state.uuidV7Ordering)));
  if (g_state.uuidV7Ordering == UUIDV7_DEDICATED_COUNTER)
    settings.push_back(NumberSetting("counterBits", g_state.uuidV7CounterBits));
  else if (g_state.uuidV7Ordering == UUIDV7_MONOTONIC_RANDOM)
    settings.push_back(
        NumberSetting("incrementBits", g_state.uuidV7IncrementBits));
}

static void AddOneTimePadSettings(std::vector<OutputSetting> &settings) {
  settings.push_back(
      TextSetting("input", OneTimePadInputName(g_state.oneTimePadInputMode)));
  switch (g_state.oneTimePadInputMode) {
  case ONE_TIME_PAD_TEXT_INPUT:
    settings.push_back(NumberSetting(
        "length",
        static_cast<long long>(strlen(g_state.oneTimePadMessage.data()))));
    break;
  case ONE_TIME_PAD_MESSAGE_LENGTH_INPUT:
    settings.push_back(NumberSetting("length", g_state.oneTimePadMessageLength));
    break;
  default: {
    const std::string path = g_state.oneTimePadFilePath.data();
    settings.push_back(
        TextSetting("file", path.substr(path.find_last_of("\\/") + 1)));
    settings.push_back(NumberSetting("bytes", g_state.oneTimePadFileSize));
    break;
  }
  }
}

std::vector<OutputSetting> CaptureOutputSettings() {
  std::vector<OutputSetting> settings;
  if (g_state.outputFormat != FORMAT_ONE_TIME_PAD)
    settings.push_back(NumberSetting("quantity", g_state.outputQuantity));

  switch (g_state.outputFormat) {
  case FORMAT_DECIMAL:
    settings.push_back(NumberSetting("digits", g_state.decimalDigits));
    break;
  case FORMAT_INTEGER:
    settings.push_back(NumberSetting("min", g_state.integerMin));
    settings.push_back(NumberSetting("max", g_state.integerMax));
    break;
  case FORMAT_RANDOM_CHARACTERS:
    settings.push_back(NumberSetting("length", g_state.randomCharactersLength));
    settings.push_back(BooleanSetting("numbers", g_state.includeNumbers));
    settings.push_back(BooleanSetting("uppercase", g_state.includeUppercase));
    settings.push_back(BooleanSetting("lowercase", g_state.includeLowercase));
    settings.push_back(BooleanSetting("special", g_state.includeSpecial));
    break;
  case FORMAT_RAW_DATA:
    AddRawDataSettings(settings);
    break;
  case FORMAT_PASSPHRASE:
    settings.push_back(NumberSetting("words", g_state.passphraseWordCount));
    settings.push_back(
        TextSetting("separator", g_state.passphraseSeparator.data()));
    break;
  case FORMAT_UUID_V4:
  case FORMAT_UUID_V7:
    AddUuidSettings(settings);
    break;
  case FORMAT_ONE_TIME_PAD:
    AddOneTimePadSettings(settings);
    break;
  }
  return settings;
}

template <typename ItemVisitor>
static void ForEachItem(const char *text, size_t length, bool numberedItems,
                        ItemVisitor visitItem) {
  size_t itemIndex = 0;
  size_t lineStart = 0;
  while (lineStart < length) {
    const char *lineEnd = static_cast<const char *>(
        memchr(text + lineStart, '\n', length - lineStart));
    const size_t lineLength =
        lineEnd ? static_cast<size_t>(lineEnd - (text + lineStart))
                : length - lineStart;
    const bool isBlankLineBetweenItems = lineLength == 0;

    if (!isBlankLineBetweenItems) {
      const char *item = text + lineStart;
      size_t itemLength = lineLength;
      if (numberedItems) {
        const std::string numberPrefix = std::to_string(itemIndex + 1) + ". ";
        if (itemLength >= numberPrefix.size() &&
            memcmp(item, numberPrefix.data(), numberPrefix.size()) == 0) {
          item += numberPrefix.size();
          itemLength -= numberPrefix.size();
        }
      }
      visitItem(itemIndex, item, itemLength);
      itemIndex++;
    }
    lineStart += lineLength + 1;
  }
}

static void AppendQuotedHex(std::vector<char> &output, const char *data,
                            size_t length) {
  Crypto::ReserveWithoutLeavingCopies(output, length * 2 + 2);
  output.push_back('"');
  for (size_t i = 0; i < length; i++) {
    const uint8_t byte = static_cast<uint8_t>(data[i]);
    output.push_back(HEX_DIGITS[byte >> 4]);
    output.push_back(HEX_DIGITS[byte & 0x0F]);
  }
  output.push_back('"');
}

static void AppendCsvField(std::vector<char> &csv, const char *field,
                           size_t length) {
  Append(csv, "\"");
  size_t runStart = 0;
  for (size_t i = 0; i < length; i++) {
    if (field[i] != '"')
      continue;
    Append(csv, field + runStart, i - runStart);
    Append(csv, "\"\"");
    runStart = i + 1;
  }
  Append(csv, field + runStart, length - runStart);
  Append(csv, "\"");
}

static void AppendCsvField(std::vector<char> &csv, const std::string &field) {
  AppendCsvField(csv, field.data(), field.size());
}

static void AppendCsvKeyOrCiphertext(std::vector<char> &csv, const char *data,
                                     size_t length, bool fileMode) {
  if (fileMode)
    AppendQuotedHex(csv, data, length);
  else
    AppendCsvField(csv, data, length);
}

std::vector<char> BuildOneTimePadCsv(const char *key, const char *ciphertext,
                                     size_t length, bool fileMode) {
  std::vector<char> csv;
  csv.reserve(length * 4);
  AppendCsvField(csv, "key");
  if (ciphertext) {
    Append(csv, ",");
    AppendCsvField(csv, "ciphertext");
  }
  Append(csv, CSV_LINE_END);

  AppendCsvKeyOrCiphertext(csv, key, length, fileMode);
  if (ciphertext) {
    Append(csv, ",");
    AppendCsvKeyOrCiphertext(csv, ciphertext, length, fileMode);
  }
  Append(csv, CSV_LINE_END);
  return csv;
}

std::vector<char> BuildOutputCsv(const GeneratedOutputDetails &details,
                                 const char *text, size_t length) {
  if (details.format == FORMAT_ONE_TIME_PAD)
    return BuildOneTimePadCsv(text, nullptr, length, false);

  std::vector<char> csv;
  csv.reserve(length * 2);
  if (details.numberedItems) {
    AppendCsvField(csv, "#");
    Append(csv, ",");
  }
  AppendCsvField(csv, details.type);
  Append(csv, CSV_LINE_END);

  ForEachItem(text, length, details.numberedItems,
              [&csv, &details](size_t index, const char *item,
                               size_t itemLength) {
                if (details.numberedItems) {
                  AppendCsvField(csv, std::to_string(index + 1));
                  Append(csv, ",");
                }
                AppendCsvField(csv, item, itemLength);
                Append(csv, CSV_LINE_END);
              });
  return csv;
}

static void AppendJsonString(std::vector<char> &json, const char *text,
                             size_t length) {
  Append(json, "\"");
  size_t runStart = 0;
  for (size_t i = 0; i < length; i++) {
    const unsigned char character = static_cast<unsigned char>(text[i]);
    const bool isControlCharacter = character < 0x20;
    if (!isControlCharacter && character != '"' && character != '\\')
      continue;

    Append(json, text + runStart, i - runStart);
    if (isControlCharacter) {
      const char unicodeEscape[] = {'\\', 'u', '0', '0',
                                    HEX_DIGITS[character >> 4],
                                    HEX_DIGITS[character & 0x0F]};
      Append(json, unicodeEscape, sizeof(unicodeEscape));
    } else {
      const char backslashEscape[] = {'\\', text[i]};
      Append(json, backslashEscape, sizeof(backslashEscape));
    }
    runStart = i + 1;
  }
  Append(json, text + runStart, length - runStart);
  Append(json, "\"");
}

static void AppendJsonString(std::vector<char> &json, const std::string &text) {
  AppendJsonString(json, text.data(), text.size());
}

static void AppendJsonMemberName(std::vector<char> &json, const char *name) {
  Append(json, "  ");
  AppendJsonString(json, name);
  Append(json, ": ");
}

static void AppendJsonTextMember(std::vector<char> &json, const char *name,
                                 const std::string &value) {
  AppendJsonMemberName(json, name);
  AppendJsonString(json, value);
  Append(json, ",\n");
}

static void AppendJsonSettings(std::vector<char> &json,
                               const std::vector<OutputSetting> &settings) {
  AppendJsonMemberName(json, "settings");
  Append(json, "{");
  for (size_t i = 0; i < settings.size(); i++) {
    Append(json, i == 0 ? "\n    " : ",\n    ");
    AppendJsonString(json, settings[i].name);
    Append(json, ": ");
    if (settings[i].isText)
      AppendJsonString(json, settings[i].value);
    else
      Append(json, settings[i].value);
  }
  Append(json, settings.empty() ? "},\n" : "\n  },\n");
}

static void AppendJsonHeader(std::vector<char> &json,
                             const GeneratedOutputDetails &details,
                             const std::string &generatedAt) {
  Append(json, "{\n");
  AppendJsonTextMember(json, "app", "TRNG");
  AppendJsonTextMember(json, "version", APP_VERSION);
  AppendJsonTextMember(json, "category", details.category);
  AppendJsonTextMember(json, "type", details.type);
  AppendJsonSettings(json, details.settings);
  AppendJsonTextMember(json, "securityMode", details.securityMode);
  AppendJsonTextMember(json, "generatedAt", generatedAt);
}

static void AppendJsonKeyOrCiphertext(std::vector<char> &json,
                                      const char *data, size_t length,
                                      bool fileMode) {
  if (fileMode)
    AppendQuotedHex(json, data, length);
  else
    AppendJsonString(json, data, length);
}

std::vector<char> BuildOneTimePadJson(const GeneratedOutputDetails &details,
                                      const std::string &generatedAt,
                                      const char *key, const char *ciphertext,
                                      size_t length, bool fileMode) {
  std::vector<char> json;
  json.reserve(length * 4);
  AppendJsonHeader(json, details, generatedAt);
  AppendJsonTextMember(json, "encoding", fileMode ? "hex" : "text");

  AppendJsonMemberName(json, "key");
  AppendJsonKeyOrCiphertext(json, key, length, fileMode);
  if (ciphertext) {
    Append(json, ",\n");
    AppendJsonMemberName(json, "ciphertext");
    AppendJsonKeyOrCiphertext(json, ciphertext, length, fileMode);
  }
  Append(json, "\n}\n");
  return json;
}

std::vector<char> BuildOutputJson(const GeneratedOutputDetails &details,
                                  const std::string &generatedAt,
                                  const char *text, size_t length) {
  if (details.format == FORMAT_ONE_TIME_PAD)
    return BuildOneTimePadJson(details, generatedAt, text, nullptr, length,
                               false);

  std::vector<char> json;
  json.reserve(length * 2);
  AppendJsonHeader(json, details, generatedAt);
  AppendJsonMemberName(json, "items");
  Append(json, "[");
  ForEachItem(text, length, details.numberedItems,
              [&json](size_t index, const char *item, size_t itemLength) {
                Append(json, index == 0 ? "\n    " : ",\n    ");
                AppendJsonString(json, item, itemLength);
              });
  Append(json, "\n  ]\n}\n");
  return json;
}
