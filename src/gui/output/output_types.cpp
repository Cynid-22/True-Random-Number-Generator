#include "output_types.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/formats/raw_data_format.h"
#include "../../logic/logic.h"
#include "imgui.h"

static const OutputType NUMBER_TYPES[] = {
    {"Decimal (0.0 - 1.0)", FORMAT_DECIMAL, -1},
    {"Integer Range", FORMAT_INTEGER, -1},
};
static const OutputType RAW_DATA_TYPES[] = {
    {"Base 2 - Binary", FORMAT_RAW_DATA, CSPRNG::RAW_BINARY},
    {"Base 8 - Octal", FORMAT_RAW_DATA, CSPRNG::RAW_OCTAL},
    {"Base 10 - Decimal", FORMAT_RAW_DATA, CSPRNG::RAW_DECIMAL},
    {"Base 12 - Duodecimal", FORMAT_RAW_DATA, CSPRNG::RAW_DUODECIMAL},
    {"Base 16 - Hexadecimal", FORMAT_RAW_DATA, CSPRNG::RAW_HEX},
    {"Base 32", FORMAT_RAW_DATA, CSPRNG::RAW_BASE32},
    {"Base 32 - Crockford", FORMAT_RAW_DATA, CSPRNG::RAW_CROCKFORD32},
    {"Base 36", FORMAT_RAW_DATA, CSPRNG::RAW_BASE36},
    {"Base 45", FORMAT_RAW_DATA, CSPRNG::RAW_BASE45},
    {"Base 58", FORMAT_RAW_DATA, CSPRNG::RAW_BASE58},
    {"Base 62", FORMAT_RAW_DATA, CSPRNG::RAW_BASE62},
    {"Base 64", FORMAT_RAW_DATA, CSPRNG::RAW_BASE64},
    {"Base 64 URL", FORMAT_RAW_DATA, CSPRNG::RAW_BASE64URL},
    {"Base 85", FORMAT_RAW_DATA, CSPRNG::RAW_BASE85},
};
static const OutputType TEXT_TYPES[] = {
    {"Random Characters", FORMAT_RANDOM_CHARACTERS, -1},
    {"Passphrase", FORMAT_PASSPHRASE, -1},
};
static const OutputType IDENTIFIER_TYPES[] = {
    {"UUID v4", FORMAT_UUID_V4, -1},
    {"UUID v7", FORMAT_UUID_V7, -1},
};
static const OutputType ENCRYPTION_TYPES[] = {
    {"One-Time Pad", FORMAT_ONE_TIME_PAD, -1},
};

static const OutputCategory OUTPUT_CATEGORIES[] = {
    {"Numbers", NUMBER_TYPES, IM_ARRAYSIZE(NUMBER_TYPES)},
    {"Raw Data", RAW_DATA_TYPES, IM_ARRAYSIZE(RAW_DATA_TYPES)},
    {"Text & Passwords", TEXT_TYPES, IM_ARRAYSIZE(TEXT_TYPES)},
    {"Identifiers", IDENTIFIER_TYPES, IM_ARRAYSIZE(IDENTIFIER_TYPES)},
    {"Encryption", ENCRYPTION_TYPES, IM_ARRAYSIZE(ENCRYPTION_TYPES)},
};
static constexpr int OUTPUT_CATEGORY_COUNT = IM_ARRAYSIZE(OUTPUT_CATEGORIES);

static int s_lastTypeInCategory[OUTPUT_CATEGORY_COUNT] = {};

int GetOutputCategoryCount() { return OUTPUT_CATEGORY_COUNT; }

const OutputCategory &GetOutputCategory(int category) {
  return OUTPUT_CATEGORIES[category];
}

void FindCurrentOutputType(int &category, int &type) {
  for (int c = 0; c < OUTPUT_CATEGORY_COUNT; c++) {
    for (int t = 0; t < OUTPUT_CATEGORIES[c].typeCount; t++) {
      const OutputType &candidate = OUTPUT_CATEGORIES[c].types[t];
      if (candidate.format == g_state.outputFormat &&
          (candidate.rawDataEncoding < 0 ||
           candidate.rawDataEncoding == g_state.rawDataEncoding)) {
        category = c;
        type = t;
        return;
      }
    }
  }
  category = 0;
  type = 0;
}

void SelectOutputType(int category, int type) {
  int previousCategory, previousType;
  FindCurrentOutputType(previousCategory, previousType);
  const OutputType &previous = OUTPUT_CATEGORIES[previousCategory].types[previousType];
  const OutputType &next = OUTPUT_CATEGORIES[category].types[type];

  int previousFormat = g_state.outputFormat;
  g_state.outputFormat = next.format;
  if (next.rawDataEncoding >= 0)
    g_state.rawDataEncoding = next.rawDataEncoding;
  s_lastTypeInCategory[category] = type;

  Logger::Log(Logger::Level::INFO, "GUI",
              "Output Format changed from '%s > %s' to '%s > %s'",
              OUTPUT_CATEGORIES[previousCategory].name, previous.name,
              OUTPUT_CATEGORIES[category].name, next.name);

  if (previousFormat == FORMAT_ONE_TIME_PAD &&
      g_state.outputFormat != FORMAT_ONE_TIME_PAD) {
    Crypto::SecureClearVector(g_state.oneTimePadMessage);
    Crypto::SecureClearVector(g_state.oneTimePadFilePath);
    g_state.oneTimePadFileSize = 0;
  }

  UpdateTargetEntropy();
}

void SelectOutputCategory(int category) {
  SelectOutputType(category, s_lastTypeInCategory[category]);
}

void RememberOutputType(int category, int type) {
  s_lastTypeInCategory[category] = type;
}
