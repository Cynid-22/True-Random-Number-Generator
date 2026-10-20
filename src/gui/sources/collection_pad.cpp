#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"
#include <algorithm>
#include <cmath>
#include <vector>

static const size_t MOUSE_TRAIL_MAX_POINTS = 4000;
static const float TRAIL_INTERPOLATION_STEP = 0.002f;
static const float KEYSTROKE_PREVIEW_HEIGHT = 60.0f;
static const float MOUSE_CANVAS_MIN_HEIGHT = 100.0f;
static const float MOUSE_CANVAS_MIN_WIDTH = 1.0f;

static bool s_focusPadWhenShown = false;

static void AppendToKeystrokePreview(char character) {
  std::vector<char> &preview = g_state.keystrokePreview;
  if (preview.size() >= AppConfig::KEYSTROKE_PREVIEW_MAX_CHARACTERS)
    preview.erase(preview.begin());
  preview.push_back(character);
}

static void CaptureTypedCharacters() {
  ImGuiIO &io = ImGui::GetIO();
  bool textBoxHasFocus = io.WantTextInput;
  if (textBoxHasFocus)
    return;
  for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
    ImWchar character = io.InputQueueCharacters[i];
    if (character >= 32 && character < 127) {
      AppendToKeystrokePreview((char)character);
    }
  }
}

static void ReportCanvasHovered(bool hovered) {
  if (g_state.mouseMovementEnabled && g_state.mouseCollector.IsRunning()) {
    g_state.mouseCollector.SetCanvasHovered(hovered);
  }
}

void FocusCollectionPadWhenShown() { s_focusPadWhenShown = true; }

void BeginCollectionPadFrame() {
  if (!g_state.isCollecting) {
    s_focusPadWhenShown = false;
    return;
  }

  CaptureTypedCharacters();
  ReportCanvasHovered(false);
}

static void RenderKeyboardPreview() {
  ImGui::SeparatorText("Keystroke Preview");

  ImGui::BeginChild("KeyOutputBox", ImVec2(0, KEYSTROKE_PREVIEW_HEIGHT), true);
  if (g_state.keystrokePreview.empty()) {
    ImGui::TextDisabled(" [ Keystrokes will appear here ] ");
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

    float availableWidth = ImGui::GetContentRegionAvail().x - 10.0f;
    const char *visibleBegin = g_state.keystrokePreview.data();
    const char *visibleEnd = visibleBegin + g_state.keystrokePreview.size();

    while (visibleBegin < visibleEnd && ImGui::CalcTextSize(visibleBegin, visibleEnd).x > availableWidth) {
      visibleBegin++;
    }

    ImGui::TextUnformatted(visibleBegin, visibleEnd);
    ImGui::PopStyleColor();
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
      ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
}

static void CaptureMouseTrail(const ImVec2 &canvasMin, const ImVec2 &canvasSize) {
  static float lastRelativeX = -1.0f, lastRelativeY = -1.0f;

  ImVec2 mousePosition = ImGui::GetMousePos();

  float relativeX = (mousePosition.x - canvasMin.x) / canvasSize.x;
  float relativeY = (mousePosition.y - canvasMin.y) / canvasSize.y;

  if (relativeX >= 0.0f && relativeX <= 1.0f && relativeY >= 0.0f && relativeY <= 1.0f) {
    if (lastRelativeX >= 0.0f && lastRelativeY >= 0.0f) {
      float dx = relativeX - lastRelativeX;
      float dy = relativeY - lastRelativeY;
      float distance = sqrtf(dx * dx + dy * dy);

      if (distance > TRAIL_INTERPOLATION_STEP) {
        int stepCount = (int)(distance / TRAIL_INTERPOLATION_STEP);
        for (int i = 0; i <= stepCount; i++) {
          float t = (float)i / (float)stepCount;
          g_state.mouseTrail.push_back({lastRelativeX + dx * t, lastRelativeY + dy * t});
        }
      } else {
        g_state.mouseTrail.push_back({relativeX, relativeY});
      }
    } else {
      g_state.mouseTrail.push_back({relativeX, relativeY});
    }

    while (g_state.mouseTrail.size() > MOUSE_TRAIL_MAX_POINTS) g_state.mouseTrail.erase(g_state.mouseTrail.begin());

    lastRelativeX = relativeX;
    lastRelativeY = relativeY;
  }
}

static const char *MouseCanvasPlaceholder() {
  if (!g_state.mouseMovementEnabled)
    return "Mouse Movement is excluded";
  if (!g_state.isCollecting)
    return "Press Start Collection, then move the mouse here";
  return "Move mouse to generate trail...";
}

static void RenderMouseCanvas() {
  ImGui::SeparatorText("Mouse Path");

  ImVec2 canvasMin = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();
  canvasSize.x = std::max(canvasSize.x, MOUSE_CANVAS_MIN_WIDTH);
  canvasSize.y = std::max(canvasSize.y, MOUSE_CANVAS_MIN_HEIGHT);

  ImGui::InvisibleButton("canvas", canvasSize);

  ImVec2 canvasMax = ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);

  bool isHovered = ImGui::IsItemHovered();
  ReportCanvasHovered(isHovered);

  if (g_state.isCollecting && g_state.mouseMovementEnabled && isHovered) {
    CaptureMouseTrail(canvasMin, canvasSize);
  }

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(20, 20, 20, 255));
  drawList->AddRect(canvasMin, canvasMax, IM_COL32(100, 100, 100, 255));

  drawList->PushClipRect(canvasMin, canvasMax, true);

  if (!g_state.mouseTrail.empty()) {
    for (const auto &point : g_state.mouseTrail) {
      float x = canvasMin.x + point.x * canvasSize.x;
      float y = canvasMin.y + point.y * canvasSize.y;
      drawList->AddCircleFilled(ImVec2(x, y), 2.0f, IM_COL32(0, 255, 0, 150));
    }
  } else {
    const char *placeholder = MouseCanvasPlaceholder();
    ImVec2 textSize = ImGui::CalcTextSize(placeholder);
    drawList->AddText(ImVec2(canvasMin.x + (canvasSize.x - textSize.x) * 0.5f, canvasMin.y + (canvasSize.y - textSize.y) * 0.5f), IM_COL32(100, 100, 100, 255), placeholder);
  }

  drawList->PopClipRect();
}

void RenderCollectionPad() {
  if (s_focusPadWhenShown) {
    ImGui::SetNextWindowFocus();
    s_focusPadWhenShown = false;
  }
  if (ImGui::BeginChild("##CollectionColumn", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)) {
    RenderKeyboardPreview();
    ImGui::Spacing();
    RenderMouseCanvas();
  }
  ImGui::EndChild();
}
