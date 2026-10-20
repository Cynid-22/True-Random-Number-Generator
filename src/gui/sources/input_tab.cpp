#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

static const float SOURCES_COLUMN_WIDTH_RATIO = 0.42f;
static const float COLUMN_GAP = 24.0f;

static const char *const STATUS_LABEL_READY = "Ready";
static const char *const STATUS_LABEL_ACTIVE = "Active";
static const char *const STATUS_LABEL_STARTING = "Starting...";
static const char *const STATUS_LABEL_UNAVAILABLE = "Unavailable";
static const char *const STATUS_LABEL_EXCLUDED = "Excluded";

static const char *UNAVAILABLE_MICROPHONE_TOOLTIP =
    "This input device/mode is currently unavailable.\n\n"
    "Possible reasons:\n"
    "- Device unplugged/disconnected\n"
    "- Device not recognized by system\n"
    "- Driver issues or missing drivers\n"
    "- Permission denied (e.g., microphone access blocked)\n"
    "- Hardware failure\n"
    "- System-level problems preventing access\n\n"
    "Past collected entropy from this source is still included in the "
    "total calculation.\n"
    "No new data is being collected from this source while "
    "unavailable.\n"
    "Resolve the issue (reconnect device, check permissions, fix "
    "drivers, etc.) to resume collection.";

static const char *SYSTEM_SOURCES_INTRO =
    "These sources exploit hardware imperfections and OS scheduling chaos. "
    "They run in parallel to intentionally create CPU 'traffic jams' for "
    "maximum entropy.";

static const char *PARALLEL_EXECUTION_EXPLANATION =
    "Sequential execution is predictable. Parallel execution forces the CPU "
    "to context-switch, causing cache contention and thermal variations.";

static void RenderExcludedStatus() {
  RenderSourceBadge(UiColor::DimText, STATUS_LABEL_EXCLUDED,
                    "This source is disabled by the user and will not "
                    "contribute to entropy collection.");
}

static void RenderMicrophoneStatus() {
  if (!g_state.microphoneEnabled) {
    RenderExcludedStatus();
  } else if (g_state.isCollecting) {
    if (g_state.microphoneCollector.IsCapturing()) {
      RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                        "Microphone is actively collecting entropy.\n"
                        "Collected: %.1f bits\n"
                        "Source is working correctly.",
                        g_state.microphoneEntropyBits);
    } else if (!g_state.microphoneCollector.HasFailed()) {
      RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_STARTING,
                        "Microphone capture is starting up.\n"
                        "Collection will begin shortly.");
    } else {
      RenderSourceBadge(UiColor::StatusUnavailable, STATUS_LABEL_UNAVAILABLE,
                        UNAVAILABLE_MICROPHONE_TOOLTIP);
    }
  } else {
    RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_READY,
                      "Microphone is enabled and ready to collect data when "
                      "collection starts.");
  }
}

static void RenderKeystrokeStatus() {
  if (!g_state.keystrokeEnabled) {
    RenderExcludedStatus();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                      "Keystroke dynamics are actively being collected.\n"
                      "Collected: %.1f bits\n"
                      "Source is working correctly.",
                      g_state.keystrokeEntropyBits);
  } else {
    RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_READY,
                      "Keystroke capture is enabled and ready to collect "
                      "data when collection starts.");
  }
}

static void RenderMouseStatus() {
  if (!g_state.mouseMovementEnabled) {
    RenderExcludedStatus();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                      "Mouse movement is actively being tracked.\n"
                      "Collected: %.1f bits\n"
                      "Source is working correctly.",
                      g_state.mouseEntropyBits);
  } else {
    RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_READY,
                      "Mouse movement tracking is enabled and ready to "
                      "collect data when collection starts.");
  }
}

static void RenderClockDriftStatus() {
  if (!g_state.clockDriftEnabled) {
    RenderExcludedStatus();
  } else if (g_state.isCollecting) {
    if (g_state.clockDriftCollector.IsRunning()) {
      RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                        "Clock Drift collector is actively running.\n\n"
                        "Samples collected: %llu\n"
                        "Collection rate: %.0f samples/sec\n"
                        "Collected entropy: %.1f bits\n"
                        "Estimated entropy per sample: ~2 bits\n"
                        "Source is working correctly.",
                        g_state.clockDriftCollector.GetSampleCount(),
                        g_state.clockDriftCollector.GetSamplesPerSecond(),
                        g_state.clockDriftEntropyBits);
    } else {
      RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_STARTING,
                        "Clock Drift collector is starting up.\n"
                        "The background thread is initializing.\n"
                        "Collection will begin shortly.");
    }
  } else {
    RenderSourceBadge(
        UiColor::StatusReady, STATUS_LABEL_READY,
        "Clock Drift collection is enabled and ready.\n"
        "The collector will start automatically when collection begins.\n"
        "This source uses CPU cycle counter measurements to extract hardware "
        "entropy.");
  }
}

static void RenderCpuJitterStatus() {
  if (!g_state.cpuJitterEnabled) {
    RenderExcludedStatus();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(
        UiColor::StatusActive, STATUS_LABEL_ACTIVE,
        "CPU Jitter race condition is actively running.\n"
        "Collected: %.1f bits\n"
        "Thread race is generating entropy from OS scheduling jitter.\n"
        "Source is working correctly.",
        g_state.cpuJitterEntropyBits);
  } else {
    RenderSourceBadge(
        UiColor::StatusReady, STATUS_LABEL_READY,
        "CPU Jitter collection is enabled and ready.\n"
        "The race condition threads will start automatically when collection "
        "begins.\n"
        "This source exploits OS scheduling unpredictability for entropy.");
  }
}

static void RenderClockDriftRate() {
  if (g_state.clockDriftEnabled && g_state.isCollecting &&
      g_state.clockDriftCollector.IsRunning()) {
    ImGui::Spacing();
    ImGui::TextColored(UiColor::LabelText,
                       "Samples: %llu | Rate: %.0f samples/sec",
                       g_state.clockDriftCollector.GetSampleCount(),
                       g_state.clockDriftCollector.GetSamplesPerSecond());
  }
}

enum class SourceGroup { UserInput, SystemInput };

struct SourceRow {
  SourceGroup group;
  const char *name;
  const char *title;
  const char *logName;
  bool AppState::*enabled;
  const char *howItWorks;
  void (*renderStatus)();
  void (*renderLiveStats)();
};

static const SourceRow SOURCE_ROWS[] = {
    {SourceGroup::UserInput, "Microphone Noise",
     "Microphone Noise (Thermal Entropy)", "Microphone",
     &AppState::microphoneEnabled,
     "Captures the Least Significant Bit (LSB) of audio samples. This bit is "
     "determined by thermal noise (electrons bouncing due to heat), not "
     "actual sound. High sample rate (44.1kHz) provides ~44,000 random "
     "bits/sec.",
     RenderMicrophoneStatus, nullptr},
    {SourceGroup::UserInput, "Keystroke Dynamics",
     "Keystroke Dynamics (User Entropy)", "Keystroke",
     &AppState::keystrokeEnabled,
     "Captures your unique typing rhythm: Flight Time (gap between keys) and "
     "Dwell Time (how long each key is held). These timings are measured in "
     "nanoseconds and are unique to each person.",
     RenderKeystrokeStatus, nullptr},
    {SourceGroup::UserInput, "Mouse Movement", "Mouse Movement (User Entropy)",
     "Mouse", &AppState::mouseMovementEnabled,
     "Records X/Y coordinates and precise timestamps (nanoseconds) of mouse "
     "movements. Your physical motor noise creates unpredictable patterns. "
     "Small movements (under 2 pixels) are filtered to avoid sensor drift.",
     RenderMouseStatus, nullptr},
    {SourceGroup::SystemInput, "Clock Drift", "Clock Drift (Hardware Entropy)",
     "Clock Drift", &AppState::clockDriftEnabled,
     "Measures the delta in CPU cycle counts during a fixed System Time "
     "window. A 3GHz CPU should count ~3,000,000 cycles per ms, but due to "
     "heat/voltage, the actual count varies (e.g., 3,000,402 or 2,999,881). "
     "Those fluctuating digits are entropy.",
     RenderClockDriftStatus, RenderClockDriftRate},
    {SourceGroup::SystemInput, "CPU Jitter", "CPU Jitter (System Entropy)",
     "CPU Jitter", &AppState::cpuJitterEnabled,
     "Creates a 'race condition': Thread A counts up infinitely, Thread B "
     "periodically freezes it and reads the count. The exact count depends "
     "on OS scheduling, background tasks (WiFi, updates), making it "
     "unpredictable.",
     RenderCpuJitterStatus, nullptr},
};
static constexpr int SOURCE_ROW_COUNT = IM_ARRAYSIZE(SOURCE_ROWS);

static int s_selectedSourceRow = 0;

static float StatusColumnWidth() {
  float width = 0.0f;
  for (const char *label :
       {STATUS_LABEL_READY, STATUS_LABEL_ACTIVE, STATUS_LABEL_STARTING,
        STATUS_LABEL_UNAVAILABLE, STATUS_LABEL_EXCLUDED})
    width = std::max(width, ImGui::CalcTextSize(label).x);
  return width;
}

static void RenderGroupRow(const char *groupName) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::TextColored(UiColor::LabelText, "%s", groupName);
}

static void RenderSourceRow(int rowIndex) {
  const SourceRow &row = SOURCE_ROWS[rowIndex];
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::PushID(rowIndex);

  if (ImGui::Selectable("##SourceRow", s_selectedSourceRow == rowIndex,
                        ImGuiSelectableFlags_SpanAllColumns |
                            ImGuiSelectableFlags_AllowOverlap,
                        ImVec2(0.0f, ImGui::GetFrameHeight()))) {
    s_selectedSourceRow = rowIndex;
  }
  ImGui::SameLine(0.0f, ImGui::GetStyle().IndentSpacing);
  RenderSourceToggle("##Include", &(g_state.*row.enabled), row.logName);
  ImGui::SameLine();
  ImGui::TextUnformatted(row.name);

  ImGui::TableSetColumnIndex(1);
  ImGui::AlignTextToFramePadding();
  row.renderStatus();

  ImGui::PopID();
}

static void RenderSourceGroup(SourceGroup group, const char *groupName) {
  RenderGroupRow(groupName);
  for (int i = 0; i < SOURCE_ROW_COUNT; i++) {
    if (SOURCE_ROWS[i].group == group)
      RenderSourceRow(i);
  }
}

static void RenderSourceTable() {
  ImGui::SeparatorText("Entropy Sources");
  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::DimText);
  ImGui::TextWrapped("%s", g_state.isCollecting
                               ? "Stop collection to change which sources "
                                 "are included."
                               : "Ticked sources are included in the final "
                                 "calculation.");
  ImGui::PopStyleColor();
  ImGui::Spacing();

  if (ImGui::BeginTable("SourcesTable", 2,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
                            ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                            StatusColumnWidth());
    ImGui::TableHeadersRow();

    RenderSourceGroup(SourceGroup::UserInput, "User Input");
    RenderSourceGroup(SourceGroup::SystemInput, "System Input");

    ImGui::EndTable();
  }
}

static void RenderSelectedSourceDetails() {
  const SourceRow &row = SOURCE_ROWS[s_selectedSourceRow];
  ImGui::Spacing();
  ImGui::SeparatorText(row.title);
  ImGui::TextWrapped("%s", row.howItWorks);
  if (row.renderLiveStats)
    row.renderLiveStats();

  if (row.group == SourceGroup::SystemInput) {
    ImGui::Spacing();
    ImGui::SeparatorText("Why Parallel Execution?");
    ImGui::TextWrapped("%s", SYSTEM_SOURCES_INTRO);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", PARALLEL_EXECUTION_EXPLANATION);
  }
}

void RenderInputTab() {
  RenderDataLockWarning();

  const float sourcesColumnWidth = std::floor(
      (ImGui::GetContentRegionAvail().x - COLUMN_GAP) * SOURCES_COLUMN_WIDTH_RATIO);
  if (ImGui::BeginChild("##SourcesColumn", ImVec2(sourcesColumnWidth, 0.0f),
                        ImGuiChildFlags_NavFlattened,
                        ImGuiWindowFlags_NoBackground)) {
    RenderSourceTable();
    RenderSelectedSourceDetails();
  }
  ImGui::EndChild();

  ImGui::SameLine(0.0f, COLUMN_GAP);

  RenderCollectionPad();
}
