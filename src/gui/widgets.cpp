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

static ImFont *s_outputFont = nullptr;
static constexpr size_t TRUNCATE_ABOVE_BYTES = 1000;
static constexpr size_t TRUNCATED_EDGE_BYTES = 500;
static constexpr unsigned char UTF8_CONTINUATION_MASK = 0xC0;
static constexpr unsigned char UTF8_CONTINUATION_BITS = 0x80;

void SetOutputFont(ImFont *font) { s_outputFont = font; }

static bool IsUtf8ContinuationByte(char byte) {
  return (static_cast<unsigned char>(byte) & UTF8_CONTINUATION_MASK) == UTF8_CONTINUATION_BITS;
}

static size_t CharacterStartAtOrBefore(const char *text, size_t position) {
  while (position > 0 && IsUtf8ContinuationByte(text[position]))
    position--;
  return position;
}

void RenderTruncatableText(const char *text, size_t length,
                           bool truncateLongText) {
  ImGui::PushFont(s_outputFont, 0.0f);
  if (truncateLongText && length > TRUNCATE_ABOVE_BYTES) {
    const size_t headEnd = CharacterStartAtOrBefore(text, TRUNCATED_EDGE_BYTES);
    const size_t tailStart = CharacterStartAtOrBefore(text, length - TRUNCATED_EDGE_BYTES);
    RenderWrappedText(text, text + headEnd);

    ImGui::PushStyleColor(ImGuiCol_Text, UiColor::DimText);
    ImGui::TextWrapped("\n ... truncated ... \n\n");
    ImGui::PopStyleColor();

    RenderWrappedText(text + tailStart, text + length);
  } else {
    RenderWrappedText(text, text + length);
  }
  ImGui::PopFont();
}

void WipeInputTextBufferKeepingItsSize(std::vector<char> &buffer) {
  SecureZeroMemory(buffer.data(), buffer.size());
}

static void WipeTextBuffer(ImVector<char> &text) {
  if (text.Data)
    SecureZeroMemory(text.Data, static_cast<size_t>(text.Capacity));
  text.clear();
}

static void WipeTextBufferKeepingItsAllocation(ImVector<char> &text) {
  if (text.Data)
    SecureZeroMemory(text.Data, static_cast<size_t>(text.Capacity));
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
  WipeTextBufferKeepingItsAllocation(context.TempBuffer);
}
