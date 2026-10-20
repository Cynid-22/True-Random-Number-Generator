#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

static const float SOURCES_COLUMN_WIDTH_RATIO = 0.34f;
static const float SOURCES_COLUMN_MAX_WIDTH_RATIO = 0.5f;
static const int SOURCE_TABLE_COLUMN_COUNT = 2;

static const char *const STATUS_LABEL_READY = "Ready";
static const char *const STATUS_LABEL_ACTIVE = "Active";
static const char *const STATUS_LABEL_STARTING = "Starting...";
static const char *const STATUS_LABEL_UNAVAILABLE = "Unavailable";
static const char *const STATUS_LABEL_EXCLUDED = "Excluded";
static const char *const STATUS_LABEL_FAILED = "Failed";

static const char *HEALTH_TEST_FAILED_TOOLTIP =
    "This source failed an SP 800-90B health test: it repeated the same "
    "value\ntoo many times in a row, or one value took up too much of a "
    "512-sample window.\n\n"
    "Its data is still mixed into the seed, but it no longer counts toward "
    "the entropy bar.\n"
    "Stop and start collection to test it again.";

static const char *UNAVAILABLE_MICROPHONE_TOOLTIP =
    "This input device/mode is currently unavailable.\n\n"
    "Possible reasons:\n"
    "- Device unplugged/disconnected\n"
    "- Device not recognized by system\n"
    "- Driver issues or missing drivers\n"
    "- Permission denied (e.g., microphone access blocked)\n"
    "- Audio format not supported (TRNG reads 16, 24 and 32-bit integer\n"
    "  and 32-bit float samples)\n"
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

static bool IsFailingHealthTest(Entropy::EntropySource source) {
  return g_state.isCollecting && g_state.entropyPool.HealthTestFailed(source);
}

static void RenderHealthTestFailedStatus() {
  RenderSourceBadge(UiColor::StatusUnavailable, STATUS_LABEL_FAILED,
                    HEALTH_TEST_FAILED_TOOLTIP);
}

static double CreditedBits(Entropy::EntropySource source) {
  return g_state.entropyPool.GetCreditedBits(source);
}

static double MaxCreditBits(Entropy::EntropySource source) {
  return Entropy::MaxCreditBitsPerSample(source);
}

static void RenderMicrophoneStatus() {
  const Entropy::EntropySource source = Entropy::EntropySource::Microphone;
  if (!g_state.microphoneEnabled) {
    RenderExcludedStatus();
  } else if (IsFailingHealthTest(source)) {
    RenderHealthTestFailedStatus();
  } else if (g_state.isCollecting) {
    if (g_state.microphoneCollector.IsCapturing()) {
      RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                        "Microphone is actively collecting entropy.\n"
                        "Credited: %.1f bits (up to %.0f bits per 64 "
                        "audio samples)\n"
                        "Source is working correctly.",
                        CreditedBits(source), MaxCreditBits(source));
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
  const Entropy::EntropySource source = Entropy::EntropySource::Keystroke;
  if (!g_state.keystrokeEnabled) {
    RenderExcludedStatus();
  } else if (IsFailingHealthTest(source)) {
    RenderHealthTestFailedStatus();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                      "Keystroke dynamics are actively being collected.\n"
                      "Credited: %.1f bits (up to %.0f bits per key "
                      "timing)\n"
                      "Source is working correctly.",
                      CreditedBits(source), MaxCreditBits(source));
  } else {
    RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_READY,
                      "Keystroke capture is enabled and ready to collect "
                      "data when collection starts.");
  }
}

static void RenderMouseStatus() {
  const Entropy::EntropySource source = Entropy::EntropySource::Mouse;
  if (!g_state.mouseMovementEnabled) {
    RenderExcludedStatus();
  } else if (IsFailingHealthTest(source)) {
    RenderHealthTestFailedStatus();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                      "Mouse movement is actively being tracked.\n"
                      "Credited: %.1f bits (up to %.0f bit per recorded "
                      "movement)\n"
                      "Source is working correctly.",
                      CreditedBits(source), MaxCreditBits(source));
  } else {
    RenderSourceBadge(UiColor::StatusReady, STATUS_LABEL_READY,
                      "Mouse movement tracking is enabled and ready to "
                      "collect data when collection starts.");
  }
}

static void RenderClockDriftStatus() {
  const Entropy::EntropySource source = Entropy::EntropySource::ClockDrift;
  if (!g_state.clockDriftEnabled) {
    RenderExcludedStatus();
  } else if (IsFailingHealthTest(source)) {
    RenderHealthTestFailedStatus();
  } else if (g_state.isCollecting) {
    if (g_state.clockDriftCollector.IsRunning()) {
      RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                        "Clock Drift collector is actively running.\n\n"
                        "Samples collected: %llu\n"
                        "Collection rate: %.0f samples/sec\n"
                        "Credited entropy: %.1f bits\n"
                        "Credited per sample: up to %.0f bit\n"
                        "Source is working correctly.",
                        g_state.clockDriftCollector.GetSampleCount(),
                        g_state.clockDriftCollector.GetSamplesPerSecond(),
                        CreditedBits(source), MaxCreditBits(source));
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
  const Entropy::EntropySource source = Entropy::EntropySource::CpuJitter;
  if (!g_state.cpuJitterEnabled) {
    RenderExcludedStatus();
  } else if (IsFailingHealthTest(source)) {
    RenderHealthTestFailedStatus();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(
        UiColor::StatusActive, STATUS_LABEL_ACTIVE,
        "CPU Jitter race condition is actively running.\n"
        "Credited: %.1f bits (up to %.0f bit per sample)\n"
        "Thread race is generating entropy from OS scheduling jitter.\n"
        "Source is working correctly.",
        CreditedBits(source), MaxCreditBits(source));
  } else {
    RenderSourceBadge(
        UiColor::StatusReady, STATUS_LABEL_READY,
        "CPU Jitter collection is enabled and ready.\n"
        "The race condition threads will start automatically when collection "
        "begins.\n"
        "This source exploits OS scheduling unpredictability for entropy.");
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
};

static const SourceRow SOURCE_ROWS[] = {
    {SourceGroup::UserInput, "Microphone Noise",
     "Microphone Noise (Thermal Entropy)", "Microphone",
     &AppState::microphoneEnabled,
     "Captures the Least Significant Bit (LSB) of audio samples. This bit is "
     "determined by thermal noise (electrons bouncing due to heat), not "
     "actual sound. Each block of 64 LSBs is credited at most 8 bits, less "
     "when its bits are unbalanced or too regular.",
     RenderMicrophoneStatus},
    {SourceGroup::UserInput, "Keystroke Dynamics",
     "Keystroke Dynamics (User Entropy)", "Keystroke",
     &AppState::keystrokeEnabled,
     "Captures your unique typing rhythm: Flight Time (gap between keys) and "
     "Dwell Time (how long each key is held). These timings are measured in "
     "nanoseconds and are unique to each person. Only keys typed while the "
     "TRNG window is in front count; auto-repeat from a held key and keys "
     "sent by software are ignored.",
     RenderKeystrokeStatus},
    {SourceGroup::UserInput, "Mouse Movement", "Mouse Movement (User Entropy)",
     "Mouse", &AppState::mouseMovementEnabled,
     "Records X/Y coordinates and precise timestamps (nanoseconds) of mouse "
     "movements. Your physical motor noise creates unpredictable patterns. "
     "Small movements (under 2 pixels) are filtered to avoid sensor drift. "
     "Only changes in speed and direction are credited, so steady straight "
     "lines count for nothing. Movement sent by software is ignored.",
     RenderMouseStatus},
    {SourceGroup::SystemInput, "Clock Drift", "Clock Drift (Hardware Entropy)",
     "Clock Drift", &AppState::clockDriftEnabled,
     "Measures the delta in CPU cycle counts during a fixed System Time "
     "window. A 3GHz CPU should count ~3,000,000 cycles per ms, but due to "
     "heat/voltage, the actual count varies (e.g., 3,000,402 or 2,999,881). "
     "Those fluctuating digits are entropy.",
     RenderClockDriftStatus},
    {SourceGroup::SystemInput, "CPU Jitter", "CPU Jitter (System Entropy)",
     "CPU Jitter", &AppState::cpuJitterEnabled,
     "Creates a 'race condition': Thread A counts up as fast as it can, and "
     "Thread B wakes about every 1.5 ms and reads how far it got. The count "
     "depends on OS scheduling and background tasks (WiFi, updates), making "
     "it unpredictable. Thread A keeps one CPU core busy while collecting.",
     RenderCpuJitterStatus},
};
static constexpr int SOURCE_ROW_COUNT = IM_ARRAYSIZE(SOURCE_ROWS);

static int s_selectedSourceRow = 0;

static float StatusColumnWidth() {
  float width = 0.0f;
  for (const char *label :
       {STATUS_LABEL_READY, STATUS_LABEL_ACTIVE, STATUS_LABEL_STARTING,
        STATUS_LABEL_UNAVAILABLE, STATUS_LABEL_EXCLUDED, STATUS_LABEL_FAILED})
    width = std::max(width, ImGui::CalcTextSize(label).x);
  return width;
}

static float SourceTableMinWidth() {
  const ImGuiStyle &style = ImGui::GetStyle();
  float nameWidth = 0.0f;
  for (const SourceRow &row : SOURCE_ROWS)
    nameWidth = std::max(nameWidth, ImGui::CalcTextSize(row.name).x);
  const float sourceColumnWidth = style.IndentSpacing + ImGui::GetFrameHeight() +
                                  style.ItemSpacing.x + nameWidth;
  const float cellPaddingWidth = style.CellPadding.x * 2.0f * SOURCE_TABLE_COLUMN_COUNT;
  const float borderLinesWidth = SOURCE_TABLE_COLUMN_COUNT + 1.0f;
  return sourceColumnWidth + StatusColumnWidth() + cellPaddingWidth +
         borderLinesWidth;
}

static float SourcesColumnWidth() {
  const float columnsWidth = ImGui::GetContentRegionAvail().x - INPUT_COLUMN_GAP;
  const float preferredWidth = std::max(
      columnsWidth * SOURCES_COLUMN_WIDTH_RATIO, SourceTableMinWidth());
  return std::floor(
      std::min(preferredWidth, columnsWidth * SOURCES_COLUMN_MAX_WIDTH_RATIO));
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

  if (row.group == SourceGroup::SystemInput) {
    ImGui::Spacing();
    ImGui::SeparatorText("Why Parallel Execution?");
    ImGui::TextWrapped("%s", SYSTEM_SOURCES_INTRO);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", PARALLEL_EXECUTION_EXPLANATION);
  }
}

void RenderInputTab() {
  RenderUsedDataWarning();

  if (ImGui::BeginChild("##SourcesColumn", ImVec2(SourcesColumnWidth(), 0.0f),
                        ImGuiChildFlags_NavFlattened,
                        ImGuiWindowFlags_NoBackground)) {
    RenderSourceTable();
    RenderSelectedSourceDetails();
  }
  ImGui::EndChild();

  ImGui::SameLine(0.0f, INPUT_COLUMN_GAP);

  RenderCollectionPad();
}
