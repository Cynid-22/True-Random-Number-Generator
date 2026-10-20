#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>

static bool RenderCharacterSetToggle(const char *label, bool *included) {
  if (ImGui::Checkbox(label, included)) {
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Custom]: %s toggled %s", label,
                *included ? "ON" : "OFF");
    return true;
  }
  return false;
}

bool RenderRandomCharactersOptions() {
  bool changed = false;

  BeginOptionRow("Length:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputInt("##CustomLen", &g_state.randomCharactersLength)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Custom]: Length set to %d",
                g_state.randomCharactersLength);
  }
  g_state.randomCharactersLength = std::clamp(g_state.randomCharactersLength,
                                              AppConfig::RANDOM_CHARACTERS_MIN_LENGTH,
                                              AppConfig::RANDOM_CHARACTERS_MAX_LENGTH);

  BeginOptionRow("Include:");
  if (RenderCharacterSetToggle("0-9", &g_state.includeNumbers))
    changed = true;
  ImGui::SameLine();
  if (RenderCharacterSetToggle("A-Z", &g_state.includeUppercase))
    changed = true;
  ImGui::SameLine();
  if (RenderCharacterSetToggle("a-z", &g_state.includeLowercase))
    changed = true;
  ImGui::SameLine();
  if (RenderCharacterSetToggle("Special", &g_state.includeSpecial))
    changed = true;
  ImGui::SameLine();
  ImGui::TextDisabled("%s", "(!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~)");

  return changed;
}

bool RenderPassphraseOptions() {
  bool changed = false;

  BeginOptionRow("Word count:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputInt("##WordCount", &g_state.passphraseWordCount)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Passphrase]: Word count set to %d",
                g_state.passphraseWordCount);
  }
  g_state.passphraseWordCount = std::clamp(g_state.passphraseWordCount,
                                           AppConfig::PASSPHRASE_MIN_WORDS,
                                           AppConfig::PASSPHRASE_MAX_WORDS);

  BeginOptionRow("Separator:");
  ImGui::SetNextItemWidth(150);
  if (ImGui::InputText("##Separator", g_state.passphraseSeparator.data(),
                       g_state.passphraseSeparator.size())) {
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Passphrase]: Separator changed");
  }

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TableSetColumnIndex(1);
  ImGui::TextColored(
      UiColor::LabelText,
      "(Using built-in wordlist: 123,565 words, ~16.9 bits/word)");

  return changed;
}
