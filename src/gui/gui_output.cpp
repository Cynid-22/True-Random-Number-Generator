#include "../../external/imgui/imgui.h"
#include <windows.h>
#include <commdlg.h>

#include "../core/app_state.h"
#include "../entropy/entropy_common.h"
#include "../logging/logger.h"
#include "../logic/csprng.h"
#include "../logic/logic.h"
#include "gui.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <time.h>
#include <utility>

static std::atomic<bool> s_isGenerating{false};
static std::mutex s_genMutex;
static CSPRNG::GenerationResult s_genResult;
static std::atomic<bool> s_genFinished{false};

//=============================================================================
// OUTPUT CONFIGURATION
//=============================================================================

// Output types are grouped into categories. Each type maps onto an
// outputFormat id (see AppState); Raw Data types also pick bitByteFormat.
struct OutputType {
  const char *name;
  int format;
  int bitByteFormat; // -1 = not applicable
};

struct OutputCategory {
  const char *name;
  const OutputType *types;
  int typeCount;
};

static const OutputType s_numberTypes[] = {
    {"Decimal (0.0 - 1.0)", 0, -1},
    {"Integer Range", 1, -1},
};
static const OutputType s_rawDataTypes[] = {
    {"Base 2 - Binary", 4, CSPRNG::RAW_BINARY},
    {"Base 8 - Octal", 4, CSPRNG::RAW_OCTAL},
    {"Base 10 - Decimal", 4, CSPRNG::RAW_DECIMAL},
    {"Base 12 - Duodecimal", 4, CSPRNG::RAW_DUODECIMAL},
    {"Base 16 - Hexadecimal", 4, CSPRNG::RAW_HEX},
    {"Base 32", 4, CSPRNG::RAW_BASE32},
    {"Base 32 - Crockford", 4, CSPRNG::RAW_CROCKFORD32},
    {"Base 36", 4, CSPRNG::RAW_BASE36},
    {"Base 45", 4, CSPRNG::RAW_BASE45},
    {"Base 58", 4, CSPRNG::RAW_BASE58},
    {"Base 62", 4, CSPRNG::RAW_BASE62},
    {"Base 64", 4, CSPRNG::RAW_BASE64},
    {"Base 64 URL", 4, CSPRNG::RAW_BASE64URL},
    {"Base 85", 4, CSPRNG::RAW_BASE85},
};

// What each Raw Data encoding is used for, and its characters
// (indexed by CSPRNG::RawEncoding)
struct RawEncodingAbout {
  const char *use;
  const char *alphabet;      // As printed by default (lowercase letters)
  const char *alphabetUpper; // With Uppercase on (single-case bases only)
};
static const RawEncodingAbout s_rawAbout[CSPRNG::RAW_ENCODING_COUNT] = {
    /* Hex */ {"Memory addresses and color codes.", "0-9, a-f", "0-9, A-F"},
    /* B64 */ {"Email attachments and embedded assets (RFC 4648).",
               "A-Z, a-z, 0-9, + and /, with = padding"},
    /* Bin */ {"How computer hardware stores and processes data.", "0, 1"},
    /* Oct */ {"File permissions on Unix systems.", "0-7"},
    /* Dec */ {"Everyday counting and arithmetic.", "0-9"},
    /* Duo */ {"Fractions (12 divides evenly by 2, 3, 4 and 6).",
               "0-9, a, b (a = ten, b = eleven)",
               "0-9, A, B (A = ten, B = eleven)"},
    /* B32 */ {"2FA authenticator secrets (RFC 4648).",
               "a-z, 2-7, with = padding", "A-Z, 2-7, with = padding"},
    /* Crk */ {"Error-tolerant, human-readable codes typed by hand.",
               "0-9, a-z without i, l, o, u", "0-9, A-Z without I, L, O, U"},
    /* B36 */ {"Short, case-insensitive IDs.", "0-9, a-z", "0-9, A-Z"},
    /* B45 */ {"Data inside QR codes (RFC 9285).",
               "0-9, a-z, space and $ % * + - . / :",
               "0-9, A-Z, space and $ % * + - . / :"},
    /* B58 */ {"Bitcoin and crypto wallet addresses; avoids look-alike "
               "characters.",
               "1-9, A-Z, a-z without 0, O, I, l"},
    /* B62 */ {"URL shorteners and database keys.", "0-9, A-Z, a-z"},
    /* B64U */ {"Web tokens (JWTs) and other values inside links "
                "(RFC 4648 Section 5).",
                "A-Z, a-z, 0-9, - and _, no padding"},
    /* B85 */ {"Compact binary data, as in Git (RFC 1924 alphabet).",
               "0-9, A-Z, a-z and ! # $ % & ( ) * + - ; < = > ? @ ^ _ ` { | } ~"},
};
static const OutputType s_textTypes[] = {
    {"Random Characters", 3, -1},
    {"Passphrase", 5, -1},
};
static const OutputType s_identifierTypes[] = {
    {"UUID v4", 7, -1},
    {"UUID v7", 8, -1},
};
static const OutputType s_encryptionTypes[] = {
    {"One-Time Pad", 6, -1},
};

static const OutputCategory s_categories[] = {
    {"Numbers", s_numberTypes, IM_ARRAYSIZE(s_numberTypes)},
    {"Raw Data", s_rawDataTypes, IM_ARRAYSIZE(s_rawDataTypes)},
    {"Text & Passwords", s_textTypes, IM_ARRAYSIZE(s_textTypes)},
    {"Identifiers", s_identifierTypes, IM_ARRAYSIZE(s_identifierTypes)},
    {"Encryption", s_encryptionTypes, IM_ARRAYSIZE(s_encryptionTypes)},
};
static constexpr int kCategoryCount = IM_ARRAYSIZE(s_categories);

// Last type picked in each category, restored when switching back to it
static int s_lastTypeInCategory[kCategoryCount] = {};

// Locate the category/type matching the current state (first entry if none).
static void FindCurrentOutputType(int &category, int &type) {
  for (int c = 0; c < kCategoryCount; c++) {
    for (int t = 0; t < s_categories[c].typeCount; t++) {
      const OutputType &ot = s_categories[c].types[t];
      if (ot.format == g_state.outputFormat &&
          (ot.bitByteFormat < 0 || ot.bitByteFormat == g_state.bitByteFormat)) {
        category = c;
        type = t;
        return;
      }
    }
  }
  category = 0;
  type = 0;
}

static void SelectOutputType(int category, int type) {
  int prevCategory, prevType;
  FindCurrentOutputType(prevCategory, prevType);
  const OutputType &prev = s_categories[prevCategory].types[prevType];
  const OutputType &next = s_categories[category].types[type];

  int prevFormat = g_state.outputFormat;
  g_state.outputFormat = next.format;
  if (next.bitByteFormat >= 0)
    g_state.bitByteFormat = next.bitByteFormat;
  s_lastTypeInCategory[category] = type;

  Logger::Log(Logger::Level::INFO, "GUI",
              "Output Format changed from '%s > %s' to '%s > %s'",
              s_categories[prevCategory].name, prev.name,
              s_categories[category].name, next.name);

  // SECURITY: Wipe OTP message buffer when switching away from OTP mode
  if (prevFormat == 6 && g_state.outputFormat != 6) {
    Crypto::SecureClearVector(g_state.otpMessage);
    Crypto::SecureClearVector(g_state.otpFilePath);
    g_state.otpFileSize = 0;
  }

  UpdateTargetEntropy();
}

// Combo width that fits `textWidth` of text plus the arrow button, at any DPI
static float ComboWidthForText(float textWidth) {
  return textWidth + ImGui::GetFrameHeight() +
         ImGui::GetStyle().FramePadding.x * 2.0f;
}

// "2^n" for large powers of two, the plain number otherwise
static std::string PowerOfTwoText(int n) {
  if (n <= 20)
    return std::to_string(1ULL << n);
  return "2^" + std::to_string(n);
}

// A highlighted value followed by its dimmed description, on one line
static void StatText(const std::string &value, const char *label) {
  ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", value.c_str());
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", label);
}

// Dimmed "(?)" after the previous item; shows `text` only when hovered
static void HelpMarker(const char *text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  ImGui::SetItemTooltip("%s", text);
}

static void StatDivider() {
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
}

// UUIDv7 rows of the output config table: batch ordering and its settings,
// key numbers for the current settings, and a "How it works" section
static void RenderUUIDv7Options(bool &parametersChanged) {
  // Batch ordering, with the chosen method's settings on the same line
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Batch ordering:");
  ImGui::TableSetColumnIndex(1);
  const char *v7Methods[] = {"Method 1: Dedicated counter",
                             "Method 2: Monotonic random",
                             "None: Fully random"};
  // Wide enough for the longest entry at any DPI
  ImGui::SetNextItemWidth(ComboWidthForText(ImGui::CalcTextSize(v7Methods[1]).x));
  if (ImGui::Combo("##V7Method", &g_state.uuidV7Method, v7Methods,
                   IM_ARRAYSIZE(v7Methods))) {
    parametersChanged = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [UUIDv7]: Ordering set to %s",
                v7Methods[g_state.uuidV7Method]);
  }

  if (g_state.uuidV7Method == 0) { // Method 1
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Counter bits:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    if (ImGui::InputInt("##V7CounterBits", &g_state.uuidV7CounterBits)) {
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Output Config [UUIDv7]: Counter bits set to %d",
                  g_state.uuidV7CounterBits);
    }
    if (g_state.uuidV7CounterBits < AppConfig::UUIDV7_MIN_COUNTER_BITS)
      g_state.uuidV7CounterBits = AppConfig::UUIDV7_MIN_COUNTER_BITS;
    if (g_state.uuidV7CounterBits > AppConfig::UUIDV7_MAX_COUNTER_BITS)
      g_state.uuidV7CounterBits = AppConfig::UUIDV7_MAX_COUNTER_BITS;
    char help[128];
    snprintf(help, sizeof(help),
             "%d-%d bits. 12 uses all of rand_a; more borrows the leftmost "
             "bits of rand_b.",
             AppConfig::UUIDV7_MIN_COUNTER_BITS,
             AppConfig::UUIDV7_MAX_COUNTER_BITS);
    HelpMarker(help);
  } else if (g_state.uuidV7Method == 1) { // Method 2
    ImGui::SameLine(0.0f, 24.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Increment:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    if (ImGui::InputInt("random bits", &g_state.uuidV7IncrementBits)) {
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Output Config [UUIDv7]: Increment bits set to %d",
                  g_state.uuidV7IncrementBits);
    }
    if (g_state.uuidV7IncrementBits < AppConfig::UUIDV7_MIN_INCREMENT_BITS)
      g_state.uuidV7IncrementBits = AppConfig::UUIDV7_MIN_INCREMENT_BITS;
    if (g_state.uuidV7IncrementBits > AppConfig::UUIDV7_MAX_INCREMENT_BITS)
      g_state.uuidV7IncrementBits = AppConfig::UUIDV7_MAX_INCREMENT_BITS;
    char help[128];
    snprintf(help, sizeof(help),
             "Each UUID adds a random amount from 1 to 2^bits (%d-%d).",
             AppConfig::UUIDV7_MIN_INCREMENT_BITS,
             AppConfig::UUIDV7_MAX_INCREMENT_BITS);
    HelpMarker(help);
  }

  // Key numbers for the current settings
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Text("Per UUID:");
  ImGui::TableSetColumnIndex(1);
  switch (g_state.uuidV7Method) {
  case 0: {
    int n = g_state.uuidV7CounterBits;
    StatText(std::to_string(74 - n), "random bits");
    StatDivider();
    // Counter starts with its top bit clear: at least 2^(n-1) increments
    StatText(PowerOfTwoText(n - 1) + "+",
             "UUIDs per millisecond before the timestamp moves forward");
    break;
  }
  case 1: {
    int k = g_state.uuidV7IncrementBits;
    StatText(std::to_string(k), "random bits per step (73 for the first UUID "
                                "of each millisecond)");
    StatDivider();
    // Starts with its top bit clear: at least 2^73 of room, steps up to 2^k
    StatText(PowerOfTwoText(73 - k) + "+",
             "UUIDs per millisecond before the timestamp moves forward");
    break;
  }
  default:
    StatText("74", "random bits");
    StatDivider();
    StatText("No", "ordering within a millisecond");
    break;
  }

  // Warnings
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(1);
  if (g_state.uuidV7Method == 0 &&
      g_state.uuidV7CounterBits >= AppConfig::UUIDV7_WARN_COUNTER_BITS) {
    // Long counter: only 74 - n bits are random in each UUID
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
    ImGui::TextWrapped(
        "Fewer random bits: with a %d-bit counter, only %d bits of each UUID "
        "are random, so UUIDs from the same millisecond are easier to guess. "
        "Use fewer than %d bits unless that does not matter.",
        g_state.uuidV7CounterBits, 74 - g_state.uuidV7CounterBits,
        AppConfig::UUIDV7_WARN_COUNTER_BITS);
    ImGui::PopStyleColor();
  }
  if (g_state.uuidV7Method == 1 &&
      g_state.uuidV7IncrementBits <= AppConfig::UUIDV7_WARN_INCREMENT_BITS) {
    // Small steps: the next UUID is the previous one plus 1..2^k
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
    ImGui::TextWrapped(
        "Easy to guess: with %d-bit increments, anyone who sees one UUID can "
        "find the next one from the same millisecond in at most %s tries. "
        "Use more than %d bits unless that does not matter.",
        g_state.uuidV7IncrementBits,
        PowerOfTwoText(g_state.uuidV7IncrementBits).c_str(),
        AppConfig::UUIDV7_WARN_INCREMENT_BITS);
    ImGui::PopStyleColor();
  }
  // v7 embeds when it was made; worth knowing before sharing one
  ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                     "Note: each UUID reveals its creation time (to the "
                     "millisecond).");

  // Explanation, collapsed by default (same style as the input sources)
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(1);
  if (ImGui::CollapsingHeader("How it works##uuidv7")) {
    ImGui::TextWrapped(
        "A UUID v7 starts with the current time in milliseconds, so sorting "
        "UUIDs puts them in the order they were created. After the version "
        "and variant markers, 74 bits remain. Batch ordering decides how "
        "they are filled, which matters when many UUIDs are made in the same "
        "millisecond:");
    ImGui::Spacing();
    ImGui::Bullet();
    ImGui::TextWrapped(
        "Method 1: Dedicated counter. A counter right after the timestamp "
        "counts the UUIDs made in each millisecond, starting from a random "
        "value. The bits after it are random for every UUID. A longer "
        "counter fits more UUIDs per millisecond but leaves fewer random "
        "bits.");
    ImGui::Bullet();
    ImGui::TextWrapped(
        "Method 2: Monotonic random. All 74 bits work as one counter that "
        "starts from a random value each millisecond. Each UUID adds a "
        "random amount, so it stays in order but the next UUID cannot be "
        "guessed. Small increments (12 bits or fewer) make the next UUID easy "
        "to guess.");
    ImGui::Bullet();
    ImGui::TextWrapped(
        "None: Fully random. All 74 bits are random for every UUID. UUIDs "
        "made in the same millisecond can come out in any order.");
    ImGui::Spacing();
    ImGui::TextWrapped(
        "If a counter runs out within one millisecond, the timestamp is "
        "moved forward by 1 ms, so a batch always stays in order.");

    ImGui::Spacing();
    ImGui::SeparatorText("Further info");
    ImGui::TextLinkOpenURL(
        "RFC 9562 Section 5.7: UUID Version 7",
        "https://www.rfc-editor.org/rfc/rfc9562.html#section-5.7");
    ImGui::SameLine();
    ImGui::TextDisabled("- the bit layout of a v7 UUID");
    ImGui::TextLinkOpenURL(
        "RFC 9562 Section 6.2: Monotonicity and Counters",
        "https://www.rfc-editor.org/rfc/rfc9562.html#section-6.2");
    ImGui::SameLine();
    ImGui::TextDisabled("- Methods 1 and 2 in detail");
  }
}

void RenderOutputConfigSection() {
  if (s_isGenerating.load())
    ImGui::BeginDisabled();
  ImGui::Text("Output Configuration");
  ImGui::Separator();
  ImGui::Spacing();

  int curCategory, curType;
  FindCurrentOutputType(curCategory, curType);
  // Track external changes too (e.g. Options > Reset Settings)
  s_lastTypeInCategory[curCategory] = curType;

  // Category selector
  // Size both combos to their longest entry (across all categories for the
  // type combo, so it doesn't jump in width) so no text is cut off
  float categoryTextW = 0.0f, typeTextW = 0.0f;
  for (int c = 0; c < kCategoryCount; c++) {
    categoryTextW = std::max(categoryTextW,
                             ImGui::CalcTextSize(s_categories[c].name).x);
    for (int t = 0; t < s_categories[c].typeCount; t++)
      typeTextW = std::max(typeTextW,
                           ImGui::CalcTextSize(s_categories[c].types[t].name).x);
  }

  ImGui::SetNextItemWidth(ComboWidthForText(categoryTextW));
  if (ImGui::BeginCombo("##Category", s_categories[curCategory].name)) {
    for (int c = 0; c < kCategoryCount; c++) {
      bool selected = (c == curCategory);
      if (ImGui::Selectable(s_categories[c].name, selected) && !selected) {
        SelectOutputType(c, s_lastTypeInCategory[c]);
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  // Type selector (within the chosen category)
  FindCurrentOutputType(curCategory, curType);
  const OutputCategory &cat = s_categories[curCategory];
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ComboWidthForText(typeTextW));
  if (ImGui::BeginCombo("##Type", cat.types[curType].name)) {
    for (int t = 0; t < cat.typeCount; t++) {
      bool selected = (t == curType);
      if (ImGui::Selectable(cat.types[t].name, selected) && !selected) {
        SelectOutputType(curCategory, t);
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Spacing();

  // Format-specific parameters
  if (ImGui::BeginTable("OutputConfigTable", 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_None);

    bool parametersChanged = false;

    switch (g_state.outputFormat) {
    case 0: // Decimal
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Decimal digits:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Digits", &g_state.decimalDigits)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Decimal]: Digits set to %d",
                    g_state.decimalDigits);
      }
      if (g_state.decimalDigits < AppConfig::DECIMAL_MIN_DIGITS)
        g_state.decimalDigits = AppConfig::DECIMAL_MIN_DIGITS;
      if (g_state.decimalDigits > AppConfig::DECIMAL_MAX_DIGITS)
        g_state.decimalDigits = AppConfig::DECIMAL_MAX_DIGITS;
      break;

    case 1: // Integer
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Minimum:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputScalar("##Min", ImGuiDataType_S64, &g_state.integerMin)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Integer]: Min set to %lld",
                    g_state.integerMin);
      }
      if (g_state.integerMin < AppConfig::INTEGER_RANGE_MIN)
        g_state.integerMin = AppConfig::INTEGER_RANGE_MIN;
      if (g_state.integerMin > AppConfig::INTEGER_RANGE_MAX)
        g_state.integerMin = AppConfig::INTEGER_RANGE_MAX;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Maximum:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputScalar("##Max", ImGuiDataType_S64, &g_state.integerMax)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Integer]: Max set to %lld",
                    g_state.integerMax);
      }
      if (g_state.integerMax < AppConfig::INTEGER_RANGE_MIN)
        g_state.integerMax = AppConfig::INTEGER_RANGE_MIN;
      if (g_state.integerMax > AppConfig::INTEGER_RANGE_MAX)
        g_state.integerMax = AppConfig::INTEGER_RANGE_MAX;
      break;

    case 3: // Custom
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Length:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##CustomLen", &g_state.customLength)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: Length set to %d",
                    g_state.customLength);
      }
      if (g_state.customLength < AppConfig::CUSTOM_MIN_LENGTH)
        g_state.customLength = AppConfig::CUSTOM_MIN_LENGTH;
      if (g_state.customLength > AppConfig::CUSTOM_MAX_LENGTH)
        g_state.customLength = AppConfig::CUSTOM_MAX_LENGTH;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Include:");
      ImGui::TableSetColumnIndex(1);
      if (ImGui::Checkbox("0-9", &g_state.includeNumbers)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: 0-9 toggled %s",
                    g_state.includeNumbers ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("A-Z", &g_state.includeUppercase)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: A-Z toggled %s",
                    g_state.includeUppercase ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("a-z", &g_state.includeLowercase)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: a-z toggled %s",
                    g_state.includeLowercase ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Special", &g_state.includeSpecial)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Custom]: Special toggled %s",
                    g_state.includeSpecial ? "ON" : "OFF");
      }
      ImGui::SameLine();
      ImGui::TextDisabled("(!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~)");
      break;

    case 4: { // Bit/Byte
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Amount:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Amount", &g_state.bitByteAmount)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Bit/Byte]: Amount set to %d",
                    g_state.bitByteAmount);
      }
      if (g_state.bitByteAmount < AppConfig::BITBYTE_MIN_AMOUNT)
        g_state.bitByteAmount = AppConfig::BITBYTE_MIN_AMOUNT;
      if (g_state.bitByteAmount > AppConfig::BITBYTE_MAX_AMOUNT)
        g_state.bitByteAmount = AppConfig::BITBYTE_MAX_AMOUNT;

      // Unit on the same line as the amount
      ImGui::SameLine();
      const char *units[] = {"Bits", "Bytes"};
      ImGui::SetNextItemWidth(ComboWidthForText(ImGui::CalcTextSize(units[1]).x));
      if (ImGui::Combo("##Unit", &g_state.bitByteUnit, units, 2)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Bit/Byte]: Unit set to %s",
                    units[g_state.bitByteUnit]);
      }

      // What one item comes out as for the current settings
      {
        const uint64_t requested =
            (g_state.bitByteUnit == 0)
                ? static_cast<uint64_t>(g_state.bitByteAmount)
                : static_cast<uint64_t>(g_state.bitByteAmount) * 8;
        const CSPRNG::RawLayout layout =
            CSPRNG::GetRawLayout(g_state.bitByteFormat, requested);
        ImGui::SameLine();
        if (layout.digits)
          ImGui::TextDisabled("(%llu digits, %.1f bits)",
                              (unsigned long long)layout.chars,
                              layout.bitsCarried);
        else if (layout.bitsCarried > static_cast<double>(requested))
          ImGui::TextDisabled("(%llu characters, rounded up to %.0f bits)",
                              (unsigned long long)layout.chars,
                              layout.bitsCarried);
        else
          ImGui::TextDisabled("(%llu characters)",
                              (unsigned long long)layout.chars);
      }

      const int enc = g_state.bitByteFormat;
      if (enc >= 0 && enc < CSPRNG::RAW_ENCODING_COUNT) {
        // What the encoding is for and which characters it uses
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("About:");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(s_rawAbout[enc].use);
        StatDivider();
        ImGui::TextDisabled("Characters:");
        ImGui::SameLine();
        ImGui::TextUnformatted(
            (CSPRNG::RawHasSingleCaseLetters(enc) && g_state.rawUppercase[enc])
                ? s_rawAbout[enc].alphabetUpper
                : s_rawAbout[enc].alphabet);

        // Grouping: a space every N characters
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Grouping:");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Checkbox("Separator", &g_state.rawGroupEnabled[enc])) {
          Logger::Log(Logger::Level::INFO, "GUI",
                      "Output Config [Raw Data]: Separator toggled %s",
                      g_state.rawGroupEnabled[enc] ? "ON" : "OFF");
        }
        if (g_state.rawGroupEnabled[enc]) {
          ImGui::SameLine();
          ImGui::SetNextItemWidth(150);
          ImGui::InputInt(enc == CSPRNG::RAW_BINARY ? "bits##GroupSize"
                                                    : "characters##GroupSize",
                          &g_state.rawGroupSize[enc]);
          if (g_state.rawGroupSize[enc] < 1)
            g_state.rawGroupSize[enc] = 1;
        }

        if (enc == CSPRNG::RAW_BASE45 && g_state.rawGroupEnabled[enc]) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
          ImGui::TextWrapped("Base 45 uses space as one of its characters, "
                             "so grouped output can't be decoded.");
          ImGui::PopStyleColor();
        }
      }
      break;
    }

    case 5: // Passphrase
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Word count:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##WordCount", &g_state.passphraseWordCount)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Passphrase]: Word count set to %d",
                    g_state.passphraseWordCount);
      }
      if (g_state.passphraseWordCount < AppConfig::PASSPHRASE_MIN_WORDS)
        g_state.passphraseWordCount = AppConfig::PASSPHRASE_MIN_WORDS;
      if (g_state.passphraseWordCount > AppConfig::PASSPHRASE_MAX_WORDS)
        g_state.passphraseWordCount = AppConfig::PASSPHRASE_MAX_WORDS;

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Separator:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(150);
      if (ImGui::InputText("##Separator", g_state.passphraseSeparator.data(),
                           g_state.passphraseSeparator.size())) {
        // InputText returns true on every char update, might be spammy?
        // User asked for "what did they change it to", so logging final state
        // or every char? Let's log but maybe user should press enter? relying
        // on standard behavior. To avoid spam, we could checking
        // ImGui::IsItemDeactivatedAfterEdit() but standard return is fine for
        // now if not typing fast.
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Passphrase]: Separator changed");
      }

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(
          ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          "(Using built-in wordlist: 123,565 words, ~16.5 bits/word)");
      break;
    }

    case 7: // UUIDv4
    case 8: // UUIDv7 (same options)
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Style:");
      ImGui::TableSetColumnIndex(1);
      if (ImGui::Checkbox("Uppercase", &g_state.uuidUppercase)) {
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [UUID]: Uppercase toggled %s",
                    g_state.uuidUppercase ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Hyphens", &g_state.uuidHyphens)) {
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [UUID]: Hyphens toggled %s",
                    g_state.uuidHyphens ? "ON" : "OFF");
      }

      if (g_state.outputFormat == 7) {
        ImGui::SameLine();
        ImGui::TextDisabled("(RFC 9562 version 4, 122 random bits each)");
      } else {
        RenderUUIDv7Options(parametersChanged);
      }
      break;

    case 6: // One-Time Pad
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Input Data:");

      ImGui::TableSetColumnIndex(1);

      // Tab bar for input method
      if (ImGui::BeginTabBar("OTPInputTabs")) {
        if (ImGui::BeginTabItem("Text Input")) {
          if (g_state.otpInputMode != 0)
            Logger::Log(Logger::Level::INFO, "GUI",
                        "Output Config [OTP]: Switched to Text Input");
          g_state.otpInputMode = 0;
          ImGui::Spacing();
          ImGui::Text("Enter your message:");
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
          ImGui::Text(
              "(Note: Only supports ASCII characters. Output will be ASCII.)");
          ImGui::PopStyleColor();
          ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                             "(Content is NOT logged)");
          if (ImGui::InputTextMultiline(
                  "##OTPMessage", g_state.otpMessage.data(),
                  g_state.otpMessage.size(), ImVec2(-1, 150))) {
            UpdateTargetEntropy();
            // SECURITY: DO NOT LOG MESSAGE CONTENT
            // SECURITY: DO NOT LOG MESSAGE CONTENT
            // Previously logged length via DEBUG, removed for hardening
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("File Input")) {
          if (g_state.otpInputMode != 1)
            Logger::Log(Logger::Level::INFO, "GUI",
                        "Output Config [OTP]: Switched to File Input");
          g_state.otpInputMode = 1;
          ImGui::Spacing();

          ImGui::Text("File to process:");
          ImGui::SetNextItemWidth(-1);
          ImGui::InputText("##OTPFilePath", g_state.otpFilePath.data(),
                           g_state.otpFilePath.size(),
                           ImGuiInputTextFlags_ReadOnly);

          if (ImGui::Button("Browse File...")) {
            OPENFILENAMEA ofn;
            char szFile[512] = {0};
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = NULL;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = "All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn) == TRUE) {
              strncpy(g_state.otpFilePath.data(), ofn.lpstrFile,
                      g_state.otpFilePath.size() - 1);
              g_state.otpFilePath[g_state.otpFilePath.size() - 1] = '\0';

              HANDLE hFile = CreateFileA(
                  g_state.otpFilePath.data(), GENERIC_READ, FILE_SHARE_READ,
                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
              if (hFile != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER size;
                if (GetFileSizeEx(hFile, &size)) {
                  g_state.otpFileSize = size.QuadPart;
                  Logger::Log(Logger::Level::INFO, "GUI",
                              "Output Config [OTP]: File loaded successfully. "
                              "Size: %lld bytes",
                              g_state.otpFileSize);
                }
                CloseHandle(hFile);
              } else {
                Logger::Log(
                    Logger::Level::ERR, "GUI",
                    "Output Config [OTP]: Failed to load file. Error code: %d",
                    GetLastError());
              }
              UpdateTargetEntropy();
            }
          }

          ImGui::Spacing();
          if (g_state.otpFileSize > 0) {
            ImGui::Text("File Size: %lld bytes", g_state.otpFileSize);
          }

          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }

      ImGui::Spacing();
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
      ImGui::TextWrapped(
          "Note: This program runs locally. Your message is safe.");
      ImGui::PopStyleColor();
      break;
    }
    }

    // Quantity and list layout (every format except One-Time Pad)
    if (g_state.outputFormat != 6) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Quantity:");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(230);
      if (ImGui::InputInt("##Quantity", &g_state.outputQuantity)) {
        parametersChanged = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config: Quantity set to %d",
                    g_state.outputQuantity);
      }
      if (g_state.outputQuantity < AppConfig::OUTPUT_MIN_QUANTITY)
        g_state.outputQuantity = AppConfig::OUTPUT_MIN_QUANTITY;
      if (g_state.outputQuantity > AppConfig::OUTPUT_MAX_QUANTITY)
        g_state.outputQuantity = AppConfig::OUTPUT_MAX_QUANTITY;

      // List options on the same line as the quantity
      ImGui::SameLine(0.0f, 24.0f);
      if (ImGui::Checkbox("Blank line between items",
                          &g_state.listBlankLine)) {
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config: Blank line between items toggled %s",
                    g_state.listBlankLine ? "ON" : "OFF");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Number items (1. 2. 3.)", &g_state.listNumbered)) {
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config: Number items toggled %s",
                    g_state.listNumbered ? "ON" : "OFF");
      }

      // Raw Data bases whose letters have one case: lowercase unless ticked
      const int enc = g_state.bitByteFormat;
      if (g_state.outputFormat == 4 && enc >= 0 &&
          enc < CSPRNG::RAW_ENCODING_COUNT &&
          CSPRNG::RawHasSingleCaseLetters(enc)) {
        ImGui::SameLine();
        if (ImGui::Checkbox("Uppercase", &g_state.rawUppercase[enc])) {
          Logger::Log(Logger::Level::INFO, "GUI",
                      "Output Config [Raw Data]: Uppercase toggled %s",
                      g_state.rawUppercase[enc] ? "ON" : "OFF");
        }
      }
    }

    ImGui::EndTable();

    if (parametersChanged) {
      UpdateTargetEntropy();
    }
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Display required entropy
  ImGui::Text("Current Entropy Requirement:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%.0f bits",
                     g_state.targetBits);

  if (g_state.targetBits <= 512.0f) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "(Minimum Base Security)");
  }

  if (s_isGenerating.load())
    ImGui::EndDisabled();
}

void RenderOutputSection() {
  ImGui::Spacing();
  ImGui::Text("Generated Output:");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Result is NOT logged)");

  ImGui::SameLine();
  // Align checkbox visually to the right side
  float avail = ImGui::GetContentRegionAvail().x;
  float textW = ImGui::CalcTextSize("Truncate display (Prevent UI Lag)").x;
  float extraW =
      ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + 15.0f;
  float estimatedCheckboxWidth = textW + extraW;
  if (avail > estimatedCheckboxWidth) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         (avail - estimatedCheckboxWidth));
  }
  ImGui::Checkbox("Truncate display (Prevent UI Lag)",
                  &g_state.truncateOutputView);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Limits visual rendering in the box below to the first 500 and last "
        "500 characters to prevent UI freezing.\nCopy/Export will still "
        "retrieve the FULL generated output intact.");
  }
  ImGui::Separator();

  // Output text box (with text wrapping for long output)
  // Use a child window with TextWrapped for proper wrapping
  // Size.y = -160.0f leaves space for the buttons and footer at the bottom
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginChild("##OutputChild", ImVec2(-1, -160.0f), true)) {
    if (!g_state.generatedOutput.empty()) {
      size_t outSize = g_state.generatedOutput.size();
      if (g_state.truncateOutputView && outSize > 1000) {
        std::string firstPart(g_state.generatedOutput.data(), 500);
        std::string lastPart(g_state.generatedOutput.data() + outSize - 500,
                             500);

        ImGui::TextWrapped("%s", firstPart.c_str());

        // Note: Dear ImGui does not support inline italics natively without
        // loading an italic font file. We will use a dimmer text color to
        // simulate the effect.
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextWrapped("\n ... truncated ... \n\n");
        ImGui::PopStyleColor();

        ImGui::TextWrapped("%s", lastPart.c_str());
      } else {
        // Using TextWrapped for native wrapping behavior (non-selectable)
        ImGui::TextWrapped("%s", g_state.generatedOutput.data());
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();

  // Buttons
  // Check entropy conditions
  bool hasMinimumEntropy = g_state.collectedBits >= 512.0f;
  bool hasFullEntropy = g_state.collectedBits >= g_state.targetBits;

  // Warning messages based on entropy level
  if (!hasMinimumEntropy) {
    // Block: Not enough entropy at all
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::Text("BLOCKED: Collect at least 512 bits before generating.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
  } else if (!hasFullEntropy) {
    // Warning: Can generate but using PSEUDO-RANDOM (expansion mode)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
    ImGui::Text("WARNING: Using PSEUDO-RANDOM mode (CSPRNG expansion). Collect "
                "more for TRUE RANDOMNESS.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }

  // Only disable if below 512 bits (minimum base security)
  if (s_isGenerating.load()) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      g_state.cancelGeneration = true;
    }
    ImGui::PopStyleColor(3);

    // Progress Wheel
    ImGui::SameLine();
    if (g_state.cancelGeneration) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                         "Cancelling generation, please wait... ");
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                         "Generation in progress, please wait... ");
    }
    ImGui::SameLine();
    static float timeOffset = 0.0f;
    timeOffset += ImGui::GetIO().DeltaTime;
    const char *spinners[] = {"|", "/", "-", "\\"};
    int spinnerIdx = static_cast<int>(timeOffset * 10.0f) % 4;
    ImGui::Text("%s", spinners[spinnerIdx]);

    // Check completion
    if (s_genFinished.load()) {
      std::lock_guard<std::mutex> lock(s_genMutex);

      if (s_genResult.success) {
        g_state.generatedOutput = std::move(s_genResult.output);
        g_state.entropyConsumed = s_genResult.entropyConsumed;

        // Generate timestamp for output
        time_t now = time(nullptr);
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S (UTC%z)",
                 localtime(&now));
        g_state.timestamp = timeStr;

        const char *modeStr =
            (s_genResult.mode == CSPRNG::GenerationMode::Consolidation)
                ? "TRUE RANDOMNESS"
                : "PSEUDO-RANDOM";
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Generated output using %s mode, consumed %.1f bits",
                    modeStr, s_genResult.entropyConsumed);
      } else {
        std::string err = "[ERROR] " + s_genResult.errorMessage;
        g_state.generatedOutput = std::vector<char>(err.begin(), err.end());
        g_state.generatedOutput.push_back('\0');
        g_state.entropyConsumed = 0.0f;
        Logger::Log(Logger::Level::ERR, "GUI", "Generation failed: %s",
                    s_genResult.errorMessage.c_str());
      }

      s_genFinished.store(false);
      s_isGenerating.store(false);
    }
  } else {
    if (!hasMinimumEntropy)
      ImGui::BeginDisabled();

    if (ImGui::Button("Generate", ImVec2(100, 0))) {
      // Check OTP mode consolidation requirement
      if (g_state.outputFormat == 6 && !hasFullEntropy) {
        // OTP requires TRUE RANDOMNESS (consolidation mode)
        std::string err =
            "[ERROR] One-Time Pad requires TRUE RANDOMNESS mode.\n"
            "Collect more entropy until the bar reaches 100%.";
        g_state.generatedOutput = std::vector<char>(err.begin(), err.end());
        g_state.generatedOutput.push_back('\0');
        g_state.entropyConsumed = 0.0f;
      } else {
        // Proceed with actual CSPRNG generation in background thread
        s_isGenerating.store(true);
        s_genFinished.store(false);
        g_state.cancelGeneration = false;

        // LOCK-IN: Lock all currently used entropy
        g_state.lockedDataTimestamp = Entropy::GetNanosecondTimestamp();
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Entropy locked at timestamp: %llu",
                    g_state.lockedDataTimestamp);

        // Report the same mode the warning above showed the user
        CSPRNG::GenerationMode mode =
            hasFullEntropy ? CSPRNG::GenerationMode::Consolidation
                           : CSPRNG::GenerationMode::Expansion;

        std::thread([mode]() {
          CSPRNG::GenerationResult result = CSPRNG::GenerateOutput(mode);
          {
            std::lock_guard<std::mutex> lock(s_genMutex);
            s_genResult = std::move(result);
          }
          s_genFinished.store(true);
        }).detach();
      }
    }

    if (!hasMinimumEntropy)
      ImGui::EndDisabled();
  }

  ImGui::SameLine();

  bool canCopy = !g_state.generatedOutput.empty();
  if (!canCopy)
    ImGui::BeginDisabled();
  if (ImGui::Button("Copy", ImVec2(80, 0))) {
    ImGui::SetClipboardText(g_state.generatedOutput.data());
  }
  if (!canCopy)
    ImGui::EndDisabled();

  ImGui::SameLine();

  if (!canCopy)
    ImGui::BeginDisabled();
  if (ImGui::Button("Clear", ImVec2(80, 0))) {
    // SECURITY: Securely wipe output data before clearing
    if (!g_state.generatedOutput.empty()) {
      SecureZeroMemory(g_state.generatedOutput.data(),
                       g_state.generatedOutput.size());
    }
    g_state.generatedOutput.clear();
    g_state.entropyConsumed = 0.0f;
    g_state.timestamp = "";

    // SECURITY: Also wipe OTP message buffer on clear
    Crypto::SecureClearVector(g_state.otpMessage);
  }
  if (!canCopy)
    ImGui::EndDisabled();

  // Note about time-based uniqueness
  ImGui::TextColored(
      ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
      "Note: The current time is mixed into the seed for uniqueness.");

  // Info footer
  if (!g_state.timestamp.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Generated at: %s",
                       g_state.timestamp.c_str());
  }
}

//=============================================================================
// LOGGING WARNING WINDOW
//=============================================================================

// Logging Warning Window removed for downgrading security warning logic
// void RenderLoggingWarningWindow() { ... }
