#include "widgets.h"
#include "imgui_internal.h"
#include <windows.h>

namespace ImStb {
#include "imstb_textedit.h"
}

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

std::string WithThousandsSeparators(uint64_t value) {
  std::string digits = std::to_string(value);
  for (int insertAt = (int)digits.size() - 3; insertAt > 0; insertAt -= 3)
    digits.insert((size_t)insertAt, ",");
  return digits;
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

static void WipeTextBuffer(ImVector<char> &text) {
  if (text.Data)
    SecureZeroMemory(text.Data, static_cast<size_t>(text.Capacity));
  text.clear();
}

void WipeTextInputBuffers() {
  if (!ImGui::GetCurrentContext())
    return;
  ImGuiContext &context = *ImGui::GetCurrentContext();
  ImGuiInputTextState &state = context.InputTextState;
  if (context.ActiveId != 0 && context.ActiveId == state.ID)
    ImGui::ClearActiveID();

  WipeTextBuffer(state.TextA);
  WipeTextBuffer(state.TextToRevertTo);
  WipeTextBuffer(state.CallbackTextBackup);
  if (state.Stb)
    SecureZeroMemory(&state.Stb->undostate, sizeof(state.Stb->undostate));
  state.TextSrc = nullptr;
  state.TextLen = 0;
  state.ID = 0;

  WipeTextBuffer(context.InputTextDeactivatedState.TextA);
  context.InputTextDeactivatedState.ID = 0;
  WipeTextBuffer(context.ClipboardHandlerData);
}
