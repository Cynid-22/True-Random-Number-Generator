#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../../logic/formats/text_formats.h"
#include "../../logic/word_lists.h"
#include "../widgets.h"
#include "imgui.h"
#include <algorithm>
#include <string>

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

static constexpr float WORD_LIST_TOOLTIP_WIDTH_EM = 28.0f;

static const WordLists::WordList &SelectedWordList() {
  return WordLists::Get(g_state.passphraseWordList);
}

static std::string WordListLabel(const WordLists::WordList &wordList) {
  if (!wordList.IsUsable())
    return std::string(wordList.name) + " (unavailable)";
  return std::string(wordList.name) + " (" +
         WithThousandsSeparators(wordList.words.size()) + " words)";
}

static void RenderWordListTooltip(const WordLists::WordList &wordList) {
  if (!ImGui::BeginItemTooltip())
    return;
  ImGui::PushTextWrapPos(ImGui::GetFontSize() * WORD_LIST_TOOLTIP_WIDTH_EM);
  if (wordList.IsUsable()) {
    ImGui::Text("%.2f bits per word, %llu-%llu characters per word",
                wordList.bitsPerWord,
                (unsigned long long)wordList.shortestWordLength,
                (unsigned long long)wordList.longestWordLength);
    ImGui::TextWrapped("Characters: %s", wordList.characters.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("%s", wordList.usage);
  } else {
    ImGui::TextWrapped("%s", wordList.loadError.c_str());
  }
  ImGui::PopTextWrapPos();
  ImGui::EndTooltip();
}

static bool RenderWordListSelector() {
  bool changed = false;
  const WordLists::WordList &selectedList = SelectedWordList();

  BeginOptionRow("Word list:");
  float labelWidth = 0.0f;
  for (int wordListId = 0; wordListId < WORD_LIST_COUNT; wordListId++)
    labelWidth = std::max(
        labelWidth,
        ImGui::CalcTextSize(WordListLabel(WordLists::Get(wordListId)).c_str()).x);
  ImGui::SetNextItemWidth(ComboWidthForText(labelWidth));

  const std::string preview = WordListLabel(selectedList);
  if (ImGui::BeginCombo("##WordList", preview.c_str())) {
    for (int wordListId = 0; wordListId < WORD_LIST_COUNT; wordListId++) {
      const WordLists::WordList &wordList = WordLists::Get(wordListId);
      const bool selected = (&wordList == &selectedList);
      const ImGuiSelectableFlags flags =
          wordList.IsUsable() ? ImGuiSelectableFlags_None
                              : ImGuiSelectableFlags_Disabled;
      if (ImGui::Selectable(WordListLabel(wordList).c_str(), selected, flags) &&
          !selected) {
        g_state.passphraseWordList = wordListId;
        changed = true;
        Logger::Log(Logger::Level::INFO, "GUI",
                    "Output Config [Passphrase]: Word list set to %s",
                    wordList.name);
      }
      RenderWordListTooltip(wordList);
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  HelpMarker("Hover a list in the menu to compare them.\n"
             "Every word is picked uniformly at random. More bits per word\n"
             "means fewer words for the same strength.");
  return changed;
}

static void RenderWordListDescription() {
  const WordLists::WordList &wordList = SelectedWordList();

  BeginOptionRow("About:", false);
  ImGui::TextWrapped("%s", wordList.usage);
  if (wordList.IsUsable()) {
    ImGui::TextDisabled("Length:");
    ImGui::SameLine();
    ImGui::Text("%llu-%llu characters per word",
                (unsigned long long)wordList.shortestWordLength,
                (unsigned long long)wordList.longestWordLength);
    InlineDivider();
    ImGui::TextDisabled("Characters:");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", wordList.characters.c_str());
  }
  if (wordList.warning) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::TextWrapped("%s", wordList.warning);
    ImGui::PopStyleColor();
  }
}

static void RenderPassphraseStrength() {
  const WordLists::WordList &wordList = SelectedWordList();

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TableSetColumnIndex(1);
  if (!wordList.IsUsable()) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::StatusUnavailable);
    ImGui::TextWrapped("%s", wordList.loadError.c_str());
    ImGui::PopStyleColor();
    return;
  }
  const int wordCount = g_state.passphraseWordCount;
  const int numberCount = g_state.passphraseNumberCount;
  std::string parts =
      std::to_string(wordCount) + (wordCount == 1 ? " word" : " words");
  if (numberCount > 0)
    parts += " + " + std::to_string(numberCount) +
             (numberCount == 1 ? " number" : " numbers");
  const double totalBits = wordCount * wordList.bitsPerWord +
                           CSPRNG::PassphraseNumberBits(wordCount, numberCount);
  ImGui::TextColored(UiColor::LabelText,
                     "(%s: %s words, %.2f bits/word; %s = %.1f bits)",
                     wordList.name,
                     WithThousandsSeparators(wordList.words.size()).c_str(),
                     wordList.bitsPerWord, parts.c_str(), totalBits);
}

static bool ListUsesDigits(const WordLists::WordList &wordList) {
  for (char digit = '0'; digit <= '9'; digit++) {
    if (wordList.usesCharacter[static_cast<unsigned char>(digit)])
      return true;
  }
  return false;
}

static void RenderNumberWarning() {
  const WordLists::WordList &wordList = SelectedWordList();
  if (!wordList.IsUsable() || g_state.passphraseNumberCount == 0 ||
      !ListUsesDigits(wordList))
    return;
  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
  ImGui::TextWrapped("Some words in this list contain digits, so an added "
                     "number may look like part of a word and some "
                     "passphrases may be read more than one way (fewer bits "
                     "than shown).");
  ImGui::PopStyleColor();
}

static void RenderSeparatorWarning() {
  const WordLists::WordList &wordList = SelectedWordList();
  if (!wordList.IsUsable() || wordList.noWordStartsAnother)
    return;

  const char *separator = g_state.passphraseSeparator.data();
  char sharedCharacter = '\0';
  for (const char *character = separator; *character != '\0'; character++) {
    if (wordList.usesCharacter[static_cast<unsigned char>(*character)]) {
      sharedCharacter = *character;
      break;
    }
  }
  if (separator[0] != '\0' && sharedCharacter == '\0')
    return;

  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
  if (separator[0] == '\0')
    ImGui::TextWrapped("No separator: with this list some passphrases may "
                       "split into words in more than one way, so they may "
                       "hold fewer bits than shown. Add a separator.");
  else
    ImGui::TextWrapped("Words in this list contain \"%c\", so the separator "
                       "may not always mark where a word ends and some "
                       "passphrases may be read more than one way (fewer bits "
                       "than shown). Use a character the list doesn't use, "
                       "such as a space.",
                       sharedCharacter);
  ImGui::PopStyleColor();
}

bool RenderPassphraseOptions() {
  bool changed = RenderWordListSelector();
  RenderWordListDescription();

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

  ImGui::SameLine();
  ImGui::TextUnformatted("Numbers:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputInt("##NumberCount", &g_state.passphraseNumberCount)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Passphrase]: Numbers set to %d",
                g_state.passphraseNumberCount);
  }
  g_state.passphraseNumberCount =
      std::clamp(g_state.passphraseNumberCount, 0,
                 g_state.passphraseWordCount *
                     AppConfig::PASSPHRASE_MAX_NUMBERS_PER_WORD);
  HelpMarker("Random digits (0-9) added at the start or end of random words,\n"
             "at most one before and one after each word, so up to twice\n"
             "the word count. Example: 4word-word7-2word.");

  BeginOptionRow("Separator:");
  ImGui::SetNextItemWidth(150);
  if (ImGui::InputText("##Separator", g_state.passphraseSeparator.data(),
                       g_state.passphraseSeparator.size())) {
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Passphrase]: Separator changed");
  }

  RenderPassphraseStrength();
  RenderSeparatorWarning();
  RenderNumberWarning();

  return changed;
}
