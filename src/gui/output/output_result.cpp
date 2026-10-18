#include "../../core/app_state.h"
#include "../../logic/logic.h"
#include "../gui.h"
#include "../widgets.h"
#include "generation_task.h"
#include "imgui.h"
#include <string>

static void RenderOutputHeader() {
  ImGui::Spacing();
  ImGui::Text("Generated Output:");
  ImGui::SameLine();
  ImGui::TextColored(UiColor::DimText, "(Result is NOT logged)");

  ImGui::SameLine();
  float availableWidth = ImGui::GetContentRegionAvail().x;
  float labelWidth = ImGui::CalcTextSize("Truncate display (Prevent UI Lag)").x;
  float boxAndPaddingWidth =
      ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + 15.0f;
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
  ImGui::Separator();
}

static void RenderWrappedText(const char *begin, const char *end) {
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(begin, end);
  ImGui::PopTextWrapPos();
}

static void RenderOutputText() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.10f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
  if (ImGui::BeginChild("##OutputChild", ImVec2(-1, -160.0f), true)) {
    if (!g_state.generatedOutput.empty()) {
      const char *text = g_state.generatedOutput.data();
      size_t length = g_state.generatedOutput.size();
      if (g_state.generatedOutput.back() == '\0')
        length--;

      if (g_state.truncateOutputView && length > 1000) {
        RenderWrappedText(text, text + 500);

        ImGui::PushStyleColor(ImGuiCol_Text, UiColor::DimText);
        ImGui::TextWrapped("\n ... truncated ... \n\n");
        ImGui::PopStyleColor();

        RenderWrappedText(text + length - 500, text + length);
      } else {
        RenderWrappedText(text, text + length);
      }
    }
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
}

static void RenderEntropyWarning(bool hasMinimumEntropy, bool hasTargetEntropy) {
  if (!hasMinimumEntropy) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
    ImGui::Text("BLOCKED: Collect at least 512 bits before generating.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
  } else if (!hasTargetEntropy) {
    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
    ImGui::Text("WARNING: Using PSEUDO-RANDOM mode (CSPRNG expansion). Collect "
                "more for TRUE RANDOMNESS.");
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
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                       "Generation in progress, please wait... ");
  }
  ImGui::SameLine();
  ImGui::Text("%s", CurrentSpinnerFrame());
}

static void RenderGenerateButton(bool hasMinimumEntropy, bool hasTargetEntropy) {
  if (!hasMinimumEntropy)
    ImGui::BeginDisabled();

  if (ImGui::Button("Generate", ImVec2(100, 0))) {
    const bool noCharacterSet =
        g_state.outputFormat == FORMAT_RANDOM_CHARACTERS &&
        !g_state.includeNumbers && !g_state.includeUppercase &&
        !g_state.includeLowercase && !g_state.includeSpecial;
    if (g_state.outputFormat == FORMAT_ONE_TIME_PAD && !hasTargetEntropy) {
      ShowOutputText("[ERROR] One-Time Pad requires TRUE RANDOMNESS mode.\n"
                     "Collect more entropy until the bar reaches 100%.");
    } else if (noCharacterSet) {
      ShowOutputText("[ERROR] Select at least one character set "
                     "(0-9, A-Z, a-z or Special).");
    } else {
      StartGeneration(hasTargetEntropy ? CSPRNG::GenerationMode::Consolidation
                                       : CSPRNG::GenerationMode::Expansion);
    }
  }

  if (!hasMinimumEntropy)
    ImGui::EndDisabled();
}

static void RenderCopyAndClearButtons() {
  bool hasOutput = !g_state.generatedOutput.empty();
  if (!hasOutput)
    ImGui::BeginDisabled();
  if (ImGui::Button("Copy", ImVec2(80, 0))) {
    ImGui::SetClipboardText(g_state.generatedOutput.data());
  }
  if (!hasOutput)
    ImGui::EndDisabled();

  ImGui::SameLine();

  const bool canClear = hasOutput && !IsGenerationRunning();
  if (!canClear)
    ImGui::BeginDisabled();
  if (ImGui::Button("Clear", ImVec2(80, 0))) {
    Crypto::SecureClearVector(g_state.generatedOutput);
    g_state.generatedAtText = "";
    g_state.generationTimeText = "";

    // Zero in place: ImGui needs the buffer size
    SecureZeroMemory(g_state.oneTimePadMessage.data(),
                     g_state.oneTimePadMessage.size());
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
      "Note: The current time is mixed into the seed for uniqueness.");

  if (!g_state.generatedAtText.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Generated at: %s",
                       g_state.generatedAtText.c_str());
  }
}

void RenderOutputSection() {
  RenderOutputHeader();
  RenderOutputText();

  bool hasMinimumEntropy = g_state.collectedEntropyBits >= 512.0f;
  bool hasTargetEntropy = g_state.collectedEntropyBits >= g_state.targetEntropyBits;

  RenderEntropyWarning(hasMinimumEntropy, hasTargetEntropy);

  if (IsGenerationRunning())
    RenderGenerationProgress();
  else
    RenderGenerateButton(hasMinimumEntropy, hasTargetEntropy);

  ImGui::SameLine();

  RenderCopyAndClearButtons();

  RenderOutputFooter();
}
