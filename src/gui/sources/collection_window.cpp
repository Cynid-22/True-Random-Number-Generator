#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include <cmath>
#include <string>
#include <vector>

static const size_t KEYSTROKE_PREVIEW_MAX_CHARACTERS = 500;
static const size_t MOUSE_TRAIL_MAX_POINTS = 4000;
static const float TRAIL_INTERPOLATION_STEP = 0.002f;

static void CaptureTypedCharacters() {
  ImGuiIO &io = ImGui::GetIO();
  for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
    ImWchar character = io.InputQueueCharacters[i];
    if (character >= 32 && character < 127) {
      g_state.keystrokePreview.push_back((char)character);
    }
  }
  if (g_state.keystrokePreview.size() > KEYSTROKE_PREVIEW_MAX_CHARACTERS) {
    size_t length = g_state.keystrokePreview.size();
    std::vector<char> recentCharacters(
        g_state.keystrokePreview.begin() + (length - KEYSTROKE_PREVIEW_MAX_CHARACTERS),
        g_state.keystrokePreview.end());
    g_state.keystrokePreview = recentCharacters;
  }
}

static void RenderCollectionStatus(const char *label, bool enabled, bool active) {
  ImGui::Text("%s:", label);
  ImGui::SameLine();
  if (!enabled) ImGui::TextColored(UiColor::DimText, "OFF");
  else if (active) ImGui::TextColored(UiColor::StatusActive, "ACTIVE");
  else ImGui::TextColored(UiColor::StatusReady, "WAIT");
}

static void RenderCollectionStatusRow() {
  ImGui::Text("Status:");

  ImGui::Columns(3, "status_columns", false);

  RenderCollectionStatus("Mic", g_state.microphoneEnabled, g_state.microphoneCollector.IsCapturing());
  ImGui::NextColumn();
  RenderCollectionStatus("Keys", g_state.keystrokeEnabled, true);
  ImGui::NextColumn();
  RenderCollectionStatus("Mouse", g_state.mouseMovementEnabled, true);
  ImGui::NextColumn();
  RenderCollectionStatus("Clock", g_state.clockDriftEnabled, g_state.clockDriftCollector.IsRunning());
  ImGui::NextColumn();
  RenderCollectionStatus("Jitter", g_state.cpuJitterEnabled, g_state.cpuJitterCollector.IsRunning());

  ImGui::Columns(1);
}

static void RenderKeyboardPreview() {
  ImGui::Text("Keyboard Output:");

  ImGui::BeginChild("KeyOutputBox", ImVec2(0, 60), true);
  if (g_state.keystrokePreview.empty()) {
    ImGui::TextDisabled(" [ Keystrokes will appear here ] ");
  } else {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));

    float availableWidth = ImGui::GetContentRegionAvail().x - 10.0f;
    std::string visibleText(g_state.keystrokePreview.begin(), g_state.keystrokePreview.end());

    while (!visibleText.empty() && ImGui::CalcTextSize(visibleText.c_str()).x > availableWidth) {
      visibleText = visibleText.substr(1);
    }

    ImGui::Text("%s", visibleText.c_str());
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

static void RenderMouseCanvas() {
  ImGui::Text("Mouse Path:");

  ImVec2 canvasMin = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();

  canvasSize.y -= 60.0f;

  if (canvasSize.y < 100.0f) canvasSize.y = 100.0f;

  ImGui::InvisibleButton("canvas", canvasSize);

  ImVec2 canvasMax = ImVec2(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);

  bool isHovered = ImGui::IsItemHovered();
  if (g_state.mouseMovementEnabled && g_state.mouseCollector.IsRunning()) {
    g_state.mouseCollector.SetCanvasHovered(isHovered);
  }

  if (isHovered) {
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
    const char *placeholder = "Move mouse to generate trail...";
    ImVec2 textSize = ImGui::CalcTextSize(placeholder);
    drawList->AddText(ImVec2(canvasMin.x + (canvasSize.x - textSize.x) * 0.5f, canvasMin.y + (canvasSize.y - textSize.y) * 0.5f), IM_COL32(100, 100, 100, 255), placeholder);
  }

  drawList->PopClipRect();
}

void RenderCollectionWindow() {
  if (!g_state.isCollecting)
    return;

  CaptureTypedCharacters();

  ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImVec2 workSize = viewport->WorkSize;

  ImVec2 windowSize = ImVec2(workSize.x * 0.8f, workSize.y * 0.82f);

  if (windowSize.x < 1000.0f) windowSize.x = 1000.0f;
  if (windowSize.y < 700.0f) windowSize.y = 700.0f;

  ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

  ImVec2 windowCenter = viewport->GetCenter();
  windowCenter.y += workSize.y * 0.05f;

  ImGui::SetNextWindowPos(windowCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::Begin("Collection Window", &g_state.isCollecting,
                   ImGuiWindowFlags_NoCollapse)) {
    RenderCollectionStatusRow();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderKeyboardPreview();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    RenderMouseCanvas();

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, UiColor::StopButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UiColor::StopButtonHovered);
    if (ImGui::Button("Stop Collection", ImVec2(180, 0))) {
      g_state.isCollecting = false;
    }
    ImGui::PopStyleColor(2);
  }
  ImGui::End();
}
