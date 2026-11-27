#include "../../core/app_state.h"
#include "../../logic/list_input.h"
#include "../../logic/logic.h"
#include "../gui.h"
#include "../widgets.h"
#include "format_options.h"
#include "generation_task.h"
#include "imgui.h"
#include "output_types.h"
#include <algorithm>

static const char *const LONGEST_OPTION_LABEL = "Entropy required:";

static float OptionLabelColumnWidth() {
  return ImGui::CalcTextSize(LONGEST_OPTION_LABEL).x +
         ImGui::GetStyle().IndentSpacing;
}

void BeginOptionRow(const char *label, bool alignToInput) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (alignToInput)
    ImGui::AlignTextToFramePadding();
  ImGui::Text("%s", label);
  ImGui::TableSetColumnIndex(1);
}

static void RenderOutputTypeSelectors() {
  int currentCategory, currentType;
  FindCurrentOutputType(currentCategory, currentType);
  RememberOutputType(currentCategory, currentType);

  float categoryTextWidth = 0.0f, typeTextWidth = 0.0f;
  for (int c = 0; c < GetOutputCategoryCount(); c++) {
    const OutputCategory &category = GetOutputCategory(c);
    categoryTextWidth = std::max(categoryTextWidth,
                                 ImGui::CalcTextSize(category.name).x);
    for (int t = 0; t < category.typeCount; t++)
      typeTextWidth = std::max(typeTextWidth,
                               ImGui::CalcTextSize(category.types[t].name).x);
  }

  ImGui::SetNextItemWidth(ComboWidthForText(categoryTextWidth));
  if (ImGui::BeginCombo("##Category", GetOutputCategory(currentCategory).name)) {
    for (int c = 0; c < GetOutputCategoryCount(); c++) {
      bool selected = (c == currentCategory);
      if (ImGui::Selectable(GetOutputCategory(c).name, selected) && !selected) {
        SelectOutputCategory(c);
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  FindCurrentOutputType(currentCategory, currentType);
  const OutputCategory &category = GetOutputCategory(currentCategory);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ComboWidthForText(typeTextWidth));
  if (ImGui::BeginCombo("##Type", category.types[currentType].name)) {
    for (int t = 0; t < category.typeCount; t++) {
      bool selected = (t == currentType);
      if (ImGui::Selectable(category.types[t].name, selected) && !selected) {
        SelectOutputType(currentCategory, t);
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

static bool RenderSelectedFormatOptions() {
  switch (g_state.outputFormat) {
  case FORMAT_DECIMAL:
    return RenderDecimalOptions();
  case FORMAT_INTEGER:
    return RenderIntegerOptions();
  case FORMAT_RANDOM_CHARACTERS:
    return RenderRandomCharactersOptions();
  case FORMAT_RAW_DATA:
    return RenderRawDataOptions();
  case FORMAT_PASSPHRASE:
    return RenderPassphraseOptions();
  case FORMAT_UUID_V4:
  case FORMAT_UUID_V7:
    return RenderUuidOptions();
  case FORMAT_ONE_TIME_PAD:
    RenderOneTimePadOptions();
    return false;
  case FORMAT_LIST_PICK:
    RenderListPickOptions();
    return false;
  case FORMAT_LIST_SHUFFLE:
    RenderListShuffleOptions();
    return false;
  }
  return false;
}

static void RenderItemLayoutCheckboxes(bool itemsJoinedOnOneLine) {
  ImGui::BeginDisabled(itemsJoinedOnOneLine);
  ImGui::Checkbox("Blank line between items", &g_state.blankLineBetweenItems);
  ImGui::SameLine();
  ImGui::Checkbox("Number items (1. 2. 3.)", &g_state.numberListItems);
  ImGui::EndDisabled();
  if (itemsJoinedOnOneLine)
    HelpMarker("With a separator, the shuffled items come out on one line, joined\n"
               "with it. Tick One item per line to get one item per line.");
}

static bool RenderListOptions() {
  bool changed = false;

  if (g_state.outputFormat == FORMAT_LIST_SHUFFLE) {
    BeginOptionRow("Layout:");
    RenderItemLayoutCheckboxes(
        !CSPRNG::ListJoinsWithLineBreaks(CSPRNG::ListFormatOf(g_state)));
    return false;
  }

  BeginOptionRow(g_state.outputFormat == FORMAT_LIST_PICK ? "Items to pick:"
                                                          : "Quantity:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputInt("##Quantity", &g_state.outputQuantity)) {
    changed = true;
  }
  g_state.outputQuantity = std::clamp(g_state.outputQuantity,
                                      AppConfig::OUTPUT_MIN_QUANTITY,
                                      AppConfig::OUTPUT_MAX_QUANTITY);

  ImGui::SameLine(0.0f, 24.0f);
  RenderItemLayoutCheckboxes(false);

  if (g_state.outputFormat == FORMAT_RAW_DATA)
    RenderRawDataCaseToggle();

  return changed;
}

static void RenderEntropyRequirementRow() {
  BeginOptionRow(LONGEST_OPTION_LABEL, false);
  ImGui::TextColored(UiColor::Highlight, "%.0f bits",
                     g_state.targetEntropyBits);

  if (g_state.targetEntropyBits <= AppConfig::MINIMUM_ENTROPY_BITS) {
    ImGui::SameLine();
    ImGui::TextColored(UiColor::DimText, "(Minimum Base Security)");
  }
  HelpMarker(SECURITY_STRENGTH_NOTE);
}

void RenderOutputConfigSection() {
  const bool generationRunning = IsGenerationRunning();
  if (generationRunning)
    ImGui::BeginDisabled();

  if (ImGui::BeginTable("OutputConfigTable", 2,
                        ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Labels", ImGuiTableColumnFlags_WidthFixed,
                            OptionLabelColumnWidth());
    ImGui::TableSetupColumn("Inputs", ImGuiTableColumnFlags_None);

    BeginOptionRow("Output type:");
    RenderOutputTypeSelectors();

    bool parametersChanged = RenderSelectedFormatOptions();

    if (g_state.outputFormat != FORMAT_ONE_TIME_PAD && RenderListOptions())
      parametersChanged = true;

    if (parametersChanged) {
      UpdateTargetEntropy();
    }

    RenderEntropyRequirementRow();

    ImGui::EndTable();
  }

  if (generationRunning)
    ImGui::EndDisabled();
}
