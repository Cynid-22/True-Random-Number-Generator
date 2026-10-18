#include "widgets.h"

float ComboWidthForText(float textWidth) {
  return textWidth + ImGui::GetFrameHeight() +
         ImGui::GetStyle().FramePadding.x * 2.0f;
}

void InlineDivider() {
  ImGui::SameLine();
  ImGui::TextDisabled("|");
  ImGui::SameLine();
}

void HelpMarker(const char *text) {
  ImGui::SameLine();
  ImGui::TextDisabled("(?)");
  ImGui::SetItemTooltip("%s", text);
}

const char *CurrentSpinnerFrame() {
  static const char *frames[] = {"|", "/", "-", "\\"};
  return frames[static_cast<int>(ImGui::GetTime() * 10.0) % 4];
}
