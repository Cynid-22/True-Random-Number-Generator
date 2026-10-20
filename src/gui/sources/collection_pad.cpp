#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

static const size_t MOUSE_TRAIL_MAX_POINTS = 4000;
static const float TRAIL_INTERPOLATION_STEP = 0.002f;
static const float KEYSTROKE_PREVIEW_HEIGHT = 60.0f;
static const float MOUSE_CANVAS_MIN_HEIGHT = 100.0f;
static const float MOUSE_CANVAS_MIN_WIDTH = 1.0f;

static const int LEVEL_METER_BLOCK_COUNT = 10;
static const int LEVEL_METER_RED_BLOCK_COUNT = 2;
static const float LEVEL_METER_FLOOR_DB = -60.0f;
static const float LEVEL_METER_FALL_DB_PER_SECOND = 24.0f;
static const float LEVEL_METER_BLOCK_WIDTH_EM = 0.6f;
static const float LEVEL_METER_BLOCK_HEIGHT_EM = 0.8f;
static const float LEVEL_METER_BLOCK_GAP_EM = 0.2f;
static const float LEVEL_METER_UNLIT_ALPHA = 0.18f;

static const char *const MICROPHONE_READOUT_TITLE = "Microphone";

static bool s_focusPadWhenShown = false;
static float s_microphoneMeterDb = LEVEL_METER_FLOOR_DB;

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

static bool MicrophoneIsCapturing() {
  return g_state.isCollecting && g_state.microphoneEnabled &&
         g_state.microphoneCollector.IsCapturing();
}

static float AmplitudeToDecibels(float amplitude) {
  return 20.0f * std::log10(amplitude);
}

static void UpdateMicrophoneMeter() {
  if (!MicrophoneIsCapturing()) {
    s_microphoneMeterDb = LEVEL_METER_FLOOR_DB;
    return;
  }
  const float peakLevel = g_state.microphoneCollector.GetInputPeakLevel();
  const float peakDb =
      peakLevel > 0.0f ? AmplitudeToDecibels(peakLevel) : LEVEL_METER_FLOOR_DB;
  const float fallenDb = s_microphoneMeterDb - LEVEL_METER_FALL_DB_PER_SECOND *
                                                   ImGui::GetIO().DeltaTime;
  s_microphoneMeterDb = std::max({peakDb, fallenDb, LEVEL_METER_FLOOR_DB});
}

void FocusCollectionPadWhenShown() { s_focusPadWhenShown = true; }

void BeginCollectionPadFrame() {
  UpdateMicrophoneMeter();
  if (!g_state.isCollecting) {
    s_focusPadWhenShown = false;
    return;
  }

  if (g_state.keystrokeEnabled)
    CaptureTypedCharacters();
  ReportCanvasHovered(false);
}

static const char *KeystrokePreviewPlaceholder() {
  if (!g_state.keystrokeEnabled)
    return " [ Keystroke Dynamics is excluded ] ";
  return " [ Keystrokes will appear here ] ";
}

static void RenderKeyboardPreview() {
  ImGui::SeparatorText("Keystroke Preview");

  ImGui::BeginChild("KeyOutputBox", ImVec2(0, KEYSTROKE_PREVIEW_HEIGHT), true);
  if (g_state.keystrokePreview.empty()) {
    ImGui::TextDisabled("%s", KeystrokePreviewPlaceholder());
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

struct LevelMeterMetrics {
  float blockWidth;
  float blockHeight;
  float blockGap;

  float Width() const {
    return LEVEL_METER_BLOCK_COUNT * blockWidth +
           (LEVEL_METER_BLOCK_COUNT - 1) * blockGap;
  }
};

static LevelMeterMetrics CurrentLevelMeterMetrics() {
  const float fontSize = ImGui::GetFontSize();
  return {std::round(fontSize * LEVEL_METER_BLOCK_WIDTH_EM),
          std::round(fontSize * LEVEL_METER_BLOCK_HEIGHT_EM),
          std::round(fontSize * LEVEL_METER_BLOCK_GAP_EM)};
}

static void IndentUnderHeading() {
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       ImGui::GetStyle().SeparatorTextPadding.x);
}

static float DigitCenterOffsetY() {
  const ImFontGlyph *digit = ImGui::GetFontBaked()->FindGlyph('0');
  return ImGui::GetStyle().FramePadding.y + (digit->Y0 + digit->Y1) * 0.5f;
}

static void RenderMicrophoneLevelMeter() {
  const LevelMeterMetrics meter = CurrentLevelMeterMetrics();
  const float rowHeight = ImGui::GetFrameHeight();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float left = std::floor(origin.x);
  const float top = std::round(origin.y + DigitCenterOffsetY() -
                               meter.blockHeight * 0.5f);
  const float dbPerBlock = -LEVEL_METER_FLOOR_DB / LEVEL_METER_BLOCK_COUNT;

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  for (int block = 0; block < LEVEL_METER_BLOCK_COUNT; block++) {
    const bool isRedBlock =
        block >= LEVEL_METER_BLOCK_COUNT - LEVEL_METER_RED_BLOCK_COUNT;
    const bool isLit =
        s_microphoneMeterDb > LEVEL_METER_FLOOR_DB + dbPerBlock * block;
    ImVec4 color =
        isRedBlock ? UiColor::StatusUnavailable : UiColor::StatusActive;
    if (!isLit)
      color.w = LEVEL_METER_UNLIT_ALPHA;

    const float blockLeft = left + block * (meter.blockWidth + meter.blockGap);
    drawList->AddRectFilled(
        ImVec2(blockLeft, top),
        ImVec2(blockLeft + meter.blockWidth, top + meter.blockHeight),
        ImGui::GetColorU32(color));
  }
  ImGui::Dummy(ImVec2(meter.Width(), rowHeight));
}

static float MicrophoneReadoutWidth() {
  const float separatorPadding = ImGui::GetStyle().SeparatorTextPadding.x;
  const float trailingLineWidth = separatorPadding;
  const float headingWidth = separatorPadding * 2.0f + trailingLineWidth +
                             ImGui::CalcTextSize(MICROPHONE_READOUT_TITLE).x;
  return std::max(separatorPadding + CurrentLevelMeterMetrics().Width(),
                  headingWidth);
}

static void RenderMicrophoneReadout() {
  ImGui::SeparatorText(MICROPHONE_READOUT_TITLE);
  IndentUnderHeading();
  if (g_state.microphoneEnabled) {
    RenderMicrophoneLevelMeter();
  } else {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Excluded");
  }
}

static std::string SampleReadoutText(uint64_t sampleCount,
                                     double samplesPerSecond) {
  const std::string countText = WithThousandsSeparators(sampleCount) +
                                (sampleCount == 1 ? " sample" : " samples");
  if (samplesPerSecond <= 0.0)
    return countText;

  const std::string countAndRateText =
      countText + " at " +
      WithThousandsSeparators((uint64_t)std::llround(samplesPerSecond)) + "/s";
  const bool rateFits = ImGui::CalcTextSize(countAndRateText.c_str()).x <=
                        ImGui::GetContentRegionAvail().x;
  return rateFits ? countAndRateText : countText;
}

template <typename Collector>
static void RenderSampleReadout(const char *title, bool enabled,
                                const Collector &collector) {
  ImGui::SeparatorText(title);
  IndentUnderHeading();
  ImGui::AlignTextToFramePadding();
  if (!enabled) {
    ImGui::TextDisabled("Excluded");
  } else if (!g_state.isCollecting) {
    ImGui::TextDisabled("Idle");
  } else if (!collector.IsRunning()) {
    ImGui::TextDisabled("Starting...");
  } else {
    const std::string text = SampleReadoutText(
        collector.GetSampleCount(), collector.GetSamplesPerSecond());
    ImGui::TextColored(UiColor::StatusActive, "%s", text.c_str());
  }
}

static void RenderLiveReadouts() {
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                      ImVec2(INPUT_COLUMN_GAP * 0.5f, 0.0f));
  if (ImGui::BeginTable("##LiveReadouts", 3)) {
    ImGui::TableSetupColumn("Microphone", ImGuiTableColumnFlags_WidthFixed,
                            MicrophoneReadoutWidth());
    ImGui::TableSetupColumn("Clock Drift", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("CPU Jitter", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    RenderMicrophoneReadout();
    ImGui::TableNextColumn();
    RenderSampleReadout("Clock Drift", g_state.clockDriftEnabled,
                        g_state.clockDriftCollector);
    ImGui::TableNextColumn();
    RenderSampleReadout("CPU Jitter", g_state.cpuJitterEnabled,
                        g_state.cpuJitterCollector);

    ImGui::EndTable();
  }
  ImGui::PopStyleVar();
}

void RenderCollectionPad() {
  if (s_focusPadWhenShown) {
    ImGui::SetNextWindowFocus();
    s_focusPadWhenShown = false;
  }
  if (ImGui::BeginChild("##CollectionColumn", ImVec2(0.0f, 0.0f),
                        ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground)) {
    RenderLiveReadouts();
    ImGui::Spacing();
    RenderKeyboardPreview();
    ImGui::Spacing();
    RenderMouseCanvas();
  }
  ImGui::EndChild();
}
