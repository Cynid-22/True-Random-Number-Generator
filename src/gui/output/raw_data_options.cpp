#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/formats/raw_data_format.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>

struct RawEncodingDescription {
  const char *usage;
  const char *characters;
  const char *uppercaseCharacters;
};

static const RawEncodingDescription RAW_ENCODING_DESCRIPTIONS[CSPRNG::RAW_ENCODING_COUNT] = {
    {"Memory addresses and color codes.", "0-9, a-f", "0-9, A-F"},
    {"Email attachments and embedded assets (RFC 4648).",
     "A-Z, a-z, 0-9, + and /, with = padding"},
    {"How computer hardware stores and processes data.", "0, 1"},
    {"File permissions on Unix systems.", "0-7"},
    {"Everyday counting and arithmetic.", "0-9"},
    {"Fractions (12 divides evenly by 2, 3, 4 and 6).",
     "0-9, a, b (a = ten, b = eleven)",
     "0-9, A, B (A = ten, B = eleven)"},
    {"2FA authenticator secrets (RFC 4648).",
     "a-z, 2-7, with = padding", "A-Z, 2-7, with = padding"},
    {"Error-tolerant, human-readable codes typed by hand.",
     "0-9, a-z without i, l, o, u", "0-9, A-Z without I, L, O, U"},
    {"Short, case-insensitive IDs.", "0-9, a-z", "0-9, A-Z"},
    {"Data inside QR codes (RFC 9285).",
     "0-9, a-z, space and $ % * + - . / :",
     "0-9, A-Z, space and $ % * + - . / :"},
    {"Bitcoin and crypto wallet addresses; avoids look-alike "
     "characters.",
     "1-9, A-Z, a-z without 0, O, I, l"},
    {"URL shorteners and database keys.", "0-9, A-Z, a-z"},
    {"Web tokens (JWTs) and other values inside links "
     "(RFC 4648 Section 5).",
     "A-Z, a-z, 0-9, - and _, no padding"},
    {"Compact binary data, as in Git (RFC 1924 alphabet).",
     "0-9, A-Z, a-z and ! # $ % & ( ) * + - ; < = > ? @ ^ _ ` { | } ~"},
};

static bool IsValidRawEncoding(int encoding) {
  return encoding >= 0 && encoding < CSPRNG::RAW_ENCODING_COUNT;
}

static void RenderItemSizeHint() {
  const uint64_t requestedBits =
      CSPRNG::RawDataBitCount(g_state.rawDataAmount, g_state.rawDataUnit);
  const CSPRNG::RawDataLayout layout =
      CSPRNG::GetRawDataLayout(g_state.rawDataEncoding, requestedBits);
  ImGui::SameLine();
  if (layout.usesRandomDigits)
    ImGui::TextDisabled("(%llu digits, %.1f bits)",
                        (unsigned long long)layout.characterCount,
                        layout.bitsCarried);
  else if (layout.bitsCarried > static_cast<double>(requestedBits))
    ImGui::TextDisabled("(%llu characters, rounded up to %.0f bits)",
                        (unsigned long long)layout.characterCount,
                        layout.bitsCarried);
  else
    ImGui::TextDisabled("(%llu characters)",
                        (unsigned long long)layout.characterCount);
}

static void RenderEncodingDescription(int encoding) {
  BeginOptionRow("About:", false);
  ImGui::TextUnformatted(RAW_ENCODING_DESCRIPTIONS[encoding].usage);
  InlineDivider();
  ImGui::TextDisabled("Characters:");
  ImGui::SameLine();
  ImGui::TextUnformatted(
      (CSPRNG::RawEncodingHasSingleCaseLetters(encoding) && g_state.rawDataUppercase[encoding])
          ? RAW_ENCODING_DESCRIPTIONS[encoding].uppercaseCharacters
          : RAW_ENCODING_DESCRIPTIONS[encoding].characters);
}

static void RenderGroupingOptions(int encoding) {
  BeginOptionRow("Grouping:");
  if (ImGui::Checkbox("Separator", &g_state.rawDataGroupingEnabled[encoding])) {
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Raw Data]: Separator toggled %s",
                g_state.rawDataGroupingEnabled[encoding] ? "ON" : "OFF");
  }
  if (g_state.rawDataGroupingEnabled[encoding]) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputInt(encoding == CSPRNG::RAW_BINARY ? "bits##GroupSize"
                                                   : "characters##GroupSize",
                    &g_state.rawDataGroupSize[encoding]);
    if (g_state.rawDataGroupSize[encoding] < 1)
      g_state.rawDataGroupSize[encoding] = 1;
  }

  if (encoding == CSPRNG::RAW_BASE45 && g_state.rawDataGroupingEnabled[encoding]) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
    ImGui::TextWrapped("Base 45 uses space as one of its characters, "
                       "so grouped output can't be decoded.");
    ImGui::PopStyleColor();
  }
}

bool RenderRawDataOptions() {
  bool changed = false;

  BeginOptionRow("Amount:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputInt("##Amount", &g_state.rawDataAmount)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Bit/Byte]: Amount set to %d",
                g_state.rawDataAmount);
  }
  g_state.rawDataAmount = std::clamp(g_state.rawDataAmount,
                                     AppConfig::RAW_DATA_MIN_AMOUNT,
                                     AppConfig::RAW_DATA_MAX_AMOUNT);

  ImGui::SameLine();
  const char *unitNames[] = {"Bits", "Bytes"};
  ImGui::SetNextItemWidth(ComboWidthForText(ImGui::CalcTextSize(unitNames[1]).x));
  if (ImGui::Combo("##Unit", &g_state.rawDataUnit, unitNames, 2)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Bit/Byte]: Unit set to %s",
                unitNames[g_state.rawDataUnit]);
  }

  RenderItemSizeHint();

  const int encoding = g_state.rawDataEncoding;
  if (IsValidRawEncoding(encoding)) {
    RenderEncodingDescription(encoding);
    RenderGroupingOptions(encoding);
  }

  return changed;
}

void RenderRawDataCaseToggle() {
  const int encoding = g_state.rawDataEncoding;
  if (IsValidRawEncoding(encoding) &&
      CSPRNG::RawEncodingHasSingleCaseLetters(encoding)) {
    ImGui::SameLine();
    if (ImGui::Checkbox("Uppercase", &g_state.rawDataUppercase[encoding])) {
      Logger::Log(Logger::Level::INFO, "GUI",
                  "Output Config [Raw Data]: Uppercase toggled %s",
                  g_state.rawDataUppercase[encoding] ? "ON" : "OFF");
    }
  }
}
