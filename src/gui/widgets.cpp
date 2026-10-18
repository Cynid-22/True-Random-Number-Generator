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

static void RenderWrappedText(const char *begin, const char *end) {
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(begin, end);
  ImGui::PopTextWrapPos();
}

void RenderTruncatableText(const char *text, size_t length,
                           bool truncateLongText) {
  if (truncateLongText && length > 1000) {
    RenderWrappedText(text, text + 500);

    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::DimText);
    ImGui::TextWrapped("\n ... truncated ... \n\n");
    ImGui::PopStyleColor();

    RenderWrappedText(text + length - 500, text + length);
  } else {
    RenderWrappedText(text, text + length);
  }
}
