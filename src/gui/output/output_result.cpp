#include "../../core/app_state.h"
#include "../../logic/formats/one_time_pad_format.h"
#include "../../logic/list_input.h"
#include "../../logic/logic.h"
#include "../../logic/word_lists.h"
#include "../../platform/unicode_path.h"
#include "../gui.h"
#include "../widgets.h"
#include "generation_task.h"
#include "one_time_pad_result.h"
#include "imgui.h"
#include <algorithm>
#include <string>

static const int OUTPUT_BOX_MIN_LINES = 6;

static void RenderGeneratedOutputMode() {
  const std::string &mode = g_state.generatedOutputDetails.securityMode;
  if (mode.empty())
    return;
  const bool isTrueRandom =
      mode == CSPRNG::ModeName(CSPRNG::GenerationMode::TrueRandom);
  InlineDivider();
  ImGui::TextColored(UiColor::LabelText, "Made in");
  ImGui::SameLine();
  ImGui::TextColored(isTrueRandom ? UiColor::StatusActive : UiColor::Warning,
                     "%s", mode.c_str());
  ImGui::SetItemTooltip("The mode that made the output shown below.\n"
                        "Next output, at the top, is the mode the next "
                        "generation will use.");
}

static void RenderOutputHeader() {
  ImGui::Spacing();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Generated Output:");
  ImGui::SameLine();
  ImGui::TextColored(UiColor::DimText, "(Result is NOT logged)");
  RenderGeneratedOutputMode();

  ImGui::SameLine();
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float labelWidth = ImGui::CalcTextSize("Truncate display (Prevent UI Lag)").x;
  float boxAndPaddingWidth =
      ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
  float checkboxWidth = labelWidth + boxAndPaddingWidth;
  if (availableWidth > checkboxWidth) {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                         (availableWidth - checkboxWidth));
  }
  ImGui::Checkbox("Truncate display (Prevent UI Lag)",
                  &g_state.truncateOutputView);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Limits visual rendering in the box below to the first 500 and last "
        "500 characters to prevent UI freezing.\nCopy/Export will still "
        "retrieve the FULL generated output intact.");
  }
}

static void RenderOutputText() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  const float footerHeight = ImGui::GetTextLineHeightWithSpacing();
  const float boxHeight =
      std::max(ImGui::GetContentRegionAvail().y - footerHeight,
               ImGui::GetTextLineHeightWithSpacing() * OUTPUT_BOX_MIN_LINES);
  if (ImGui::BeginChild("##OutputChild", ImVec2(-1, boxHeight), true)) {
    if (g_state.hasOneTimePadResult()) {
      RenderOneTimePadResult();
    } else if (!g_state.generatedOutput.empty()) {
      size_t length = g_state.generatedOutput.size();
      if (g_state.generatedOutput.back() == '\0')
        length--;
      RenderTruncatableText(g_state.generatedOutput.data(), length,
                            g_state.truncateOutputView);
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

static void RenderEntropyWarning(bool hasMinimumEntropy, bool canUseTruePath) {
  if (!hasMinimumEntropy) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::Text("BLOCKED: Collect at least %.0f bits before generating.",
                AppConfig::MINIMUM_ENTROPY_BITS);
    ImGui::PopStyleColor();
    ImGui::Spacing();
  } else if (!canUseTruePath) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::Text("WARNING: The next output will be PSEUDO-RANDOM: the true random "
                "reserve doesn't cover it. Collect more for TRUE RANDOM.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }
}

static void RenderGenerationProgress() {
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
  if (ImGui::Button("Cancel", ImVec2(100, 0))) {
    g_state.cancelGenerationRequested = true;
  }
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  if (g_state.cancelGenerationRequested) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "Cancelling generation, please wait... ");
  } else {
    ImGui::TextColored(UiColor::InProgress,
                       "Generation in progress, please wait... ");
  }
  ImGui::SameLine();
  ImGui::Text("%s", CurrentSpinnerFrame());
}

static const char *FindOneTimePadInputError() {
  if (g_state.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT &&
      g_state.oneTimePadMessage[0] == '\0')
    return "Enter a message first.";

  if (g_state.oneTimePadInputMode != ONE_TIME_PAD_FILE_INPUT)
    return nullptr;
  const char *path = g_state.oneTimePadFilePath.data();
  if (path[0] == '\0')
    return "Browse for a file first.";
  Platform::FileStamp currentStamp;
  if (!Platform::FileStampOf(path, currentStamp))
    return "Failed to open the input file.";
  const Platform::FileStamp selectedStamp{g_state.oneTimePadFileSize,
                                          g_state.oneTimePadFileLastWriteTime};
  if (currentStamp != selectedStamp)
    return CSPRNG::ONE_TIME_PAD_FILE_CHANGED_ERROR;
  if (currentStamp.size == 0)
    return "The selected file is empty.";
  return nullptr;
}

static std::string FindErrorPreventingGeneration(bool canUseTruePath) {
  if (g_state.outputFormat == FORMAT_RANDOM_CHARACTERS &&
      !g_state.includeNumbers && !g_state.includeUppercase &&
      !g_state.includeLowercase && !g_state.includeSpecial)
    return "Select at least one character set (0-9, A-Z, a-z or Special).";

  if (g_state.outputFormat == FORMAT_PASSPHRASE) {
    const WordLists::WordList &wordList =
        WordLists::Get(g_state.passphraseWordList);
    if (!wordList.IsUsable())
      return wordList.loadError;
  }

  if (IsListFormat(g_state.outputFormat))
    return CSPRNG::ListInputError(g_state);

  if (g_state.outputFormat != FORMAT_ONE_TIME_PAD)
    return "";
  if (const char *inputError = FindOneTimePadInputError())
    return inputError;
  if (!canUseTruePath)
    return "One-Time Pad requires TRUE RANDOM mode.\n"
           "Collect more entropy until the true random reserve bar reaches "
           "100%.\n"
           "Each pad uses up the reserve bits it draws; they never count "
           "again.";
  return "";
}

static void RenderGenerateButton(bool hasMinimumEntropy, bool canUseTruePath) {
  const bool canGenerate = hasMinimumEntropy && !g_state.selfTestFailed;
  if (!canGenerate)
    ImGui::BeginDisabled();

  if (ImGui::Button("Generate", ImVec2(100, 0))) {
    const std::string error = FindErrorPreventingGeneration(canUseTruePath);
    if (!error.empty()) {
      ShowOutputText("[ERROR] " + error);
    } else {
      StartGeneration();
    }
  }
  if (g_state.selfTestFailed &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("Generating is turned off: the cryptographic self-test "
                      "failed at launch.");

  if (!canGenerate)
    ImGui::EndDisabled();
}

static void RenderCopyAndClearButtons() {
  const bool hasOutput = !g_state.generatedOutput.empty();
  const bool isOneTimePadFileInput =
      g_state.outputFormat == FORMAT_ONE_TIME_PAD &&
      g_state.oneTimePadInputMode == ONE_TIME_PAD_FILE_INPUT;
  const bool canCopy = hasOutput && !isOneTimePadFileInput;
  if (!canCopy)
    ImGui::BeginDisabled();
  if (ImGui::Button("Copy", ImVec2(80, 0))) {
    size_t length = g_state.generatedOutput.size();
    if (length > 0 && g_state.generatedOutput.back() == '\0')
      length--;
    CopySecretText(g_state.generatedOutput.data(), length);
  }
  RenderSecretCopyTooltip();
  if (!canCopy)
    ImGui::EndDisabled();

  ImGui::SameLine();

  const bool canClear = (hasOutput || g_state.hasOneTimePadResult()) &&
                        !IsGenerationRunning();
  if (!canClear)
    ImGui::BeginDisabled();
  if (ImGui::Button("Clear", ImVec2(80, 0))) {
    g_state.wipeGeneratedOutput();
    g_state.wipeOneTimePadResult();
    g_state.generatedAtText = "";
    g_state.generationTimeText = "";

    WipeInputTextBufferKeepingItsSize(g_state.oneTimePadMessage);
    WipeTextInputBuffers();
    UpdateTargetEntropy();
  }
  if (!canClear)
    ImGui::EndDisabled();

  if (!g_state.generationTimeText.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(UiColor::DimText, "Took %s",
                       g_state.generationTimeText.c_str());
  }
}

static void RenderOutputFooter() {
  ImGui::TextColored(
      UiColor::DimText,
      "Note: The current time and a generation counter are mixed into every "
      "generation for uniqueness.");
}

void RenderOutputSection() {
  bool hasMinimumEntropy = g_state.hasMinimumEntropy();
  bool canUseTruePath = g_state.canUseTruePath();

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  if (IsGenerationRunning())
    RenderGenerationProgress();
  else
    RenderGenerateButton(hasMinimumEntropy, canUseTruePath);

  ImGui::SameLine();

  RenderCopyAndClearButtons();

  RenderEntropyWarning(hasMinimumEntropy, canUseTruePath);

  RenderOutputHeader();
  RenderOutputText();
  RenderOutputFooter();
}
