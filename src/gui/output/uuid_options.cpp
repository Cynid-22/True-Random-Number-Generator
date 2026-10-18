#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <string>

static std::string PowerOfTwoText(int exponent) {
  if (exponent <= 20)
    return std::to_string(1ULL << exponent);
  return "2^" + std::to_string(exponent);
}

static void HighlightedStat(const std::string &value, const char *label) {
  ImGui::TextColored(UiColor::Highlight, "%s", value.c_str());
  ImGui::SameLine(0.0f, 4.0f);
  ImGui::TextColored(UiColor::LabelText, "%s", label);
}

static bool RenderOrderingMethodRow() {
  bool changed = false;

  BeginOptionRow("Batch ordering:");
  const char *orderingNames[] = {"Method 1: Dedicated counter",
                                 "Method 2: Monotonic random",
                                 "None: Fully random"};
  ImGui::SetNextItemWidth(ComboWidthForText(ImGui::CalcTextSize(orderingNames[1]).x));
  if (ImGui::Combo("##V7Method", &g_state.uuidV7Ordering, orderingNames,
                   IM_ARRAYSIZE(orderingNames))) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [UUIDv7]: Ordering set to %s",
                orderingNames[g_state.uuidV7Ordering]);
  }

  if (g_state.uuidV7Ordering == UUIDV7_DEDICATED_COUNTER) {
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
    g_state.uuidV7CounterBits = std::clamp(g_state.uuidV7CounterBits,
                                           AppConfig::UUIDV7_MIN_COUNTER_BITS,
                                           AppConfig::UUIDV7_MAX_COUNTER_BITS);
    char helpText[128];
    snprintf(helpText, sizeof(helpText),
             "%d-%d bits. 12 uses all of rand_a; more borrows the leftmost "
             "bits of rand_b.",
             AppConfig::UUIDV7_MIN_COUNTER_BITS,
             AppConfig::UUIDV7_MAX_COUNTER_BITS);
    HelpMarker(helpText);
  } else if (g_state.uuidV7Ordering == UUIDV7_MONOTONIC_RANDOM) {
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
    g_state.uuidV7IncrementBits = std::clamp(g_state.uuidV7IncrementBits,
                                             AppConfig::UUIDV7_MIN_INCREMENT_BITS,
                                             AppConfig::UUIDV7_MAX_INCREMENT_BITS);
    char helpText[128];
    snprintf(helpText, sizeof(helpText),
             "Each UUID adds a random amount from 1 to 2^bits (%d-%d).",
             AppConfig::UUIDV7_MIN_INCREMENT_BITS,
             AppConfig::UUIDV7_MAX_INCREMENT_BITS);
    HelpMarker(helpText);
  }

  return changed;
}

static void RenderPerUuidStats() {
  BeginOptionRow("Per UUID:", false);
  switch (g_state.uuidV7Ordering) {
  case UUIDV7_DEDICATED_COUNTER: {
    int counterBits = g_state.uuidV7CounterBits;
    HighlightedStat(std::to_string(74 - counterBits), "random bits");
    InlineDivider();
    HighlightedStat(PowerOfTwoText(counterBits - 1) + "+",
                    "UUIDs per millisecond before the timestamp moves forward");
    break;
  }
  case UUIDV7_MONOTONIC_RANDOM: {
    int incrementBits = g_state.uuidV7IncrementBits;
    HighlightedStat(std::to_string(incrementBits),
                    "random bits per step (73 for the first UUID "
                    "of each millisecond)");
    InlineDivider();
    HighlightedStat(PowerOfTwoText(73 - incrementBits) + "+",
                    "UUIDs per millisecond before the timestamp moves forward");
    break;
  }
  default:
    HighlightedStat("74", "random bits");
    InlineDivider();
    HighlightedStat("No", "ordering within a millisecond");
    break;
  }
}

static void RenderUuidV7Warnings() {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(1);
  if (g_state.uuidV7Ordering == UUIDV7_DEDICATED_COUNTER &&
      g_state.uuidV7CounterBits >= AppConfig::UUIDV7_WARN_COUNTER_BITS) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::TextWrapped(
        "Fewer random bits: with a %d-bit counter, only %d bits of each UUID "
        "are random, so UUIDs from the same millisecond are easier to guess. "
        "Use fewer than %d bits unless that does not matter.",
        g_state.uuidV7CounterBits, 74 - g_state.uuidV7CounterBits,
        AppConfig::UUIDV7_WARN_COUNTER_BITS);
    ImGui::PopStyleColor();
  }
  if (g_state.uuidV7Ordering == UUIDV7_MONOTONIC_RANDOM &&
      g_state.uuidV7IncrementBits <= AppConfig::UUIDV7_WARN_INCREMENT_BITS) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::TextWrapped(
        "Easy to guess: with %d-bit increments, anyone who sees one UUID can "
        "find the next one from the same millisecond in at most %s tries. "
        "Use more than %d bits unless that does not matter.",
        g_state.uuidV7IncrementBits,
        PowerOfTwoText(g_state.uuidV7IncrementBits).c_str(),
        AppConfig::UUIDV7_WARN_INCREMENT_BITS);
    ImGui::PopStyleColor();
  }
  ImGui::TextColored(UiColor::Warning,
                     "Note: each UUID reveals its creation time (to the "
                     "millisecond).");
}

static void RenderUuidV7Explanation() {
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

static bool RenderUuidV7Options() {
  bool changed = RenderOrderingMethodRow();
  RenderPerUuidStats();
  RenderUuidV7Warnings();
  RenderUuidV7Explanation();
  return changed;
}

bool RenderUuidOptions() {
  BeginOptionRow("Style:");
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

  if (g_state.outputFormat == FORMAT_UUID_V4) {
    ImGui::SameLine();
    ImGui::TextDisabled("(RFC 9562 version 4, 122 random bits each)");
    return false;
  }
  return RenderUuidV7Options();
}
