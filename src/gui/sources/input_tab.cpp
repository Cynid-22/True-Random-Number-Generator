#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>

static const float SOURCES_COLUMN_WIDTH_RATIO = 0.34f;
static const float SOURCES_COLUMN_MAX_WIDTH_RATIO = 0.5f;
static const int SOURCE_TABLE_COLUMN_COUNT = 2;

static const char *const STATUS_LABEL_READY = "Ready";
static const char *const STATUS_LABEL_ACTIVE = "Active";
static const char *const STATUS_LABEL_STARTING = "Starting...";
static const char *const STATUS_LABEL_UNAVAILABLE = "Unavailable";
static const char *const STATUS_LABEL_EXCLUDED = "Excluded";
static const char *const STATUS_LABEL_FAILED = "Failed";
static const char *const STATUS_LABEL_NO_CREDIT = "No credit";

static const char *HEALTH_TEST_FAILED_TOOLTIP =
    "This source failed an SP 800-90B health test: it repeated the same "
    "value\ntoo many times in a row, or one value took up too much of a "
    "512-sample window.\n\n"
    "Its data is still mixed into the seed, but it no longer counts toward "
    "the entropy bar.\n"
    "Turn it off, or test it again, with the buttons below the source.";

static const char *HEALTH_TEST_FAILED_NOTICE =
    "Failed its health test, so its credit has stopped.";

static const char *STARTUP_TEST_TOOLTIP_FORMAT =
    "Start-up health test: %s of %s samples.\n"
    "Credit held: %.1f bits, released once the test passes.\n"
    "If the test fails, the held credit is discarded.";

static const char *VIRTUAL_MACHINE_NO_CREDIT_TOOLTIP_FORMAT =
    "TRNG is running in a virtual machine.\n"
    "This source's timing comes from the virtual machine's view of the CPU,\n"
    "which the host controls, so it earns no credit here.\n"
    "Its data is still mixed into the seed.\n\n"
    "Start-up health test: %s";

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

static const char *RAW_STREAM_LABEL =
    "raw (Windows sound enhancements skipped)";
static const char *STANDARD_STREAM_LABEL =
    "standard (raw mode unavailable, Windows sound enhancements may apply)";

static const char *SYSTEM_SOURCES_INTRO =
    "These sources measure timing inside the computer: how late Windows wakes "
    "a waiting thread, and how much work a busy thread gets done in between. "
    "Both run at the same time and share the CPU with every other program, so "
    "interrupts, other programs and the CPU's power saving all shift their "
    "timing.";

static const char *SYSTEM_SOURCES_CREDIT_NOTE =
    "How much the timing varies depends on the CPU, Windows and what else is "
    "running, so each sample is credited well below what was measured on a "
    "real computer. In a virtual machine the host controls this timing, so "
    "neither source earns credit there.";

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

static bool RenderStartupStatusIfTesting(Entropy::EntropySource source) {
  const Entropy::StartupStatus startup =
      g_state.entropyPool.GetStartupStatus(source);
  if (startup.complete)
    return false;
  RenderSourceBadge(
      UiColor::StatusReady, STATUS_LABEL_STARTING, STARTUP_TEST_TOOLTIP_FORMAT,
      WithThousandsSeparators(startup.samplesTested).c_str(),
      WithThousandsSeparators(Entropy::STARTUP_SAMPLE_COUNT).c_str(),
      startup.heldBits);
  return true;
}

static std::string StartupTestSummary(Entropy::EntropySource source) {
  if (!g_state.isCollecting)
    return "runs when collection starts";
  const Entropy::StartupStatus startup =
      g_state.entropyPool.GetStartupStatus(source);
  if (!startup.complete)
    return WithThousandsSeparators(startup.samplesTested) + " of " +
           WithThousandsSeparators(Entropy::STARTUP_SAMPLE_COUNT) + " samples";
  return "passed";
}

static void RenderNoCreditStatus(Entropy::EntropySource source) {
  RenderSourceBadge(UiColor::Warning, STATUS_LABEL_NO_CREDIT,
                    VIRTUAL_MACHINE_NO_CREDIT_TOOLTIP_FORMAT,
                    StartupTestSummary(source).c_str());
}

static std::string StartupSkippedNote(Entropy::EntropySource source) {
  const Entropy::StartupStatus startup =
      g_state.entropyPool.GetStartupStatus(source);
  if (!startup.finishedEarly)
    return "";
  return "\nStart-up test skipped: credit was released after " +
         WithThousandsSeparators(startup.samplesTested) + " of " +
         WithThousandsSeparators(Entropy::STARTUP_SAMPLE_COUNT) + " samples.";
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
      if (RenderStartupStatusIfTesting(source))
        return;
      RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                        "Microphone is actively collecting entropy.\n"
                        "Stream: %s\n"
                        "Reading the LSB of %u-bit samples\n"
                        "Credited: %.1f bits (up to %.0f bits per 64 "
                        "audio samples)\n"
                        "Source is working correctly.",
                        g_state.microphoneCollector.IsRawStream()
                            ? RAW_STREAM_LABEL
                            : STANDARD_STREAM_LABEL,
                        g_state.microphoneCollector.GetResolutionBits(),
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
    if (RenderStartupStatusIfTesting(source))
      return;
    RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                      "Keystroke dynamics are actively being collected.\n"
                      "Credited: %.1f bits (up to %.0f bits per key "
                      "timing)\n"
                      "Source is working correctly.%s",
                      CreditedBits(source), MaxCreditBits(source),
                      StartupSkippedNote(source).c_str());
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
    if (RenderStartupStatusIfTesting(source))
      return;
    RenderSourceBadge(UiColor::StatusActive, STATUS_LABEL_ACTIVE,
                      "Mouse movement is actively being tracked.\n"
                      "Credited: %.1f bits (up to %.0f bit per recorded "
                      "movement)\n"
                      "Source is working correctly.%s",
                      CreditedBits(source), MaxCreditBits(source),
                      StartupSkippedNote(source).c_str());
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
  } else if (!Entropy::IsCreditedOnThisMachine(source)) {
    RenderNoCreditStatus(source);
  } else if (g_state.isCollecting) {
    if (g_state.clockDriftCollector.IsRunning()) {
      if (RenderStartupStatusIfTesting(source))
        return;
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
        "This source times Windows' 1 ms timer with the CPU's cycle counter.");
  }
}

static void RenderCpuJitterStatus() {
  const Entropy::EntropySource source = Entropy::EntropySource::CpuJitter;
  if (!g_state.cpuJitterEnabled) {
    RenderExcludedStatus();
  } else if (IsFailingHealthTest(source)) {
    RenderHealthTestFailedStatus();
  } else if (!Entropy::IsCreditedOnThisMachine(source)) {
    RenderNoCreditStatus(source);
  } else if (g_state.isCollecting) {
    if (RenderStartupStatusIfTesting(source))
      return;
    RenderSourceBadge(
        UiColor::StatusActive, STATUS_LABEL_ACTIVE,
        "CPU Jitter threads are actively running.\n"
        "Credited: %.1f bits (up to %.0f bit per sample)\n"
        "The counting thread's progress between wake-ups varies with "
        "interrupts and scheduling.\n"
        "Source is working correctly.",
        CreditedBits(source), MaxCreditBits(source));
  } else {
    RenderSourceBadge(
        UiColor::StatusReady, STATUS_LABEL_READY,
        "CPU Jitter collection is enabled and ready.\n"
        "The counting and sampling threads will start automatically when "
        "collection begins.\n"
        "This source measures how far a busy thread counts between timer "
        "wake-ups.");
  }
}

enum class SourceGroup { UserInput, SystemInput };

struct SourceRow {
  SourceGroup group;
  Entropy::EntropySource source;
  const char *name;
  const char *title;
  const char *logName;
  bool AppState::*enabled;
  const char *howItWorks;
  void (*renderStatus)();
};

static const SourceRow SOURCE_ROWS[] = {
    {SourceGroup::UserInput, Entropy::EntropySource::Microphone,
     "Microphone Noise",
     "Microphone Noise (Hardware Entropy)", "Microphone",
     &AppState::microphoneEnabled,
     "Captures the Least Significant Bit (LSB) of audio samples, at the "
     "microphone's own bit depth. In a quiet room this bit is set mostly by "
     "electronic noise in the microphone, its amplifier and the converter. "
     "That noise is normally larger than one step of the converter, so "
     "the bit changes unpredictably from sample to sample. Sound moves the "
     "signal too, but the noise stays on top of it; only an input loud "
     "enough to clip removes it, which the credit rule and health tests "
     "catch. Each block of 64 LSBs is credited at most 8 bits, less when "
     "its bits are unbalanced or too regular.",
     RenderMicrophoneStatus},
    {SourceGroup::UserInput, Entropy::EntropySource::Keystroke,
     "Keystroke Dynamics",
     "Keystroke Dynamics (User Entropy)", "Keystroke",
     &AppState::keystrokeEnabled,
     "Captures your typing rhythm: Flight Time (gap between keys) and Dwell "
     "Time (how long each key is held), measured in nanoseconds. Nobody "
     "types with the same timing to the millisecond, and each key also "
     "reaches TRNG after a delay that changes by microseconds from key to "
     "key. The credit rests on that small delay, which watching or recording "
     "the typing does not reveal. Keys are read through "
     "Windows Raw Input on a dedicated thread, so each time is taken the "
     "moment the key arrives. Windows only passes keys to TRNG while its "
     "window is in front; auto-repeat from a held key and keys sent by "
     "software are ignored.",
     RenderKeystrokeStatus},
    {SourceGroup::UserInput, Entropy::EntropySource::Mouse, "Mouse Movement",
     "Mouse Movement (User Entropy)",
     "Mouse", &AppState::mouseMovementEnabled,
     "Records the mouse sensor's raw movement counts through Windows Raw "
     "Input, before pointer speed and acceleration (for a precision touchpad, "
     "the movement Windows derives from your finger), while the pointer is "
     "over the Mouse Path box and TRNG is in front. Your physical motor noise "
     "creates unpredictable patterns. Movement is summed into at most one "
     "sample every 7 ms, so a fast gaming mouse earns no more than a standard "
     "one. Only changes in speed and direction are credited, so steady "
     "straight lines count for nothing. Movement sent by software is ignored; "
     "touchpad movement counts only while the touchpad reports a finger on "
     "it.",
     RenderMouseStatus},
    {SourceGroup::SystemInput, Entropy::EntropySource::ClockDrift,
     "Clock Drift", "Clock Drift (Timer Entropy)",
     "Clock Drift", &AppState::clockDriftEnabled,
     "Asks Windows to wake this thread after 1 ms and counts how many ticks "
     "of the CPU's cycle counter actually passed. On modern CPUs this "
     "counter ticks at a fixed rate, so heat and voltage do not change it. "
     "The count varies because the wake-up always comes a little late, by "
     "an amount set by the timer hardware, other interrupts, the scheduler "
     "and the CPU waking from power saving. The lowest digits of the count "
     "are the entropy. The name comes from comparing two clocks: Windows' "
     "timer and the CPU's counter.",
     RenderClockDriftStatus},
    {SourceGroup::SystemInput, Entropy::EntropySource::CpuJitter, "CPU Jitter",
     "CPU Jitter (Scheduling Entropy)",
     "CPU Jitter", &AppState::cpuJitterEnabled,
     "Two threads run at once. One adds to a counter as fast as it can; the "
     "other wakes about every millisecond and records how far the counter "
     "moved since its last look. Neither thread pauses the other. The amount "
     "depends on when the second thread actually wakes and on how fast the "
     "first one ran in between: interrupts, other programs sharing its "
     "core, clock speed changes and the counter's memory moving between the "
     "two cores all change it. The counting thread keeps one CPU core busy "
     "while collecting.",
     RenderCpuJitterStatus},
};
static constexpr int SOURCE_ROW_COUNT = IM_ARRAYSIZE(SOURCE_ROWS);

static int s_selectedSourceRow = 0;

static float StatusColumnWidth() {
  float width = 0.0f;
  for (const char *label :
       {STATUS_LABEL_READY, STATUS_LABEL_ACTIVE, STATUS_LABEL_STARTING,
        STATUS_LABEL_UNAVAILABLE, STATUS_LABEL_EXCLUDED, STATUS_LABEL_FAILED,
        STATUS_LABEL_NO_CREDIT})
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

static void RenderHealthTestFailureRow(int rowIndex) {
  const SourceRow &row = SOURCE_ROWS[rowIndex];
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Indent();
  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::StatusUnavailable);
  ImGui::TextWrapped("%s", HEALTH_TEST_FAILED_NOTICE);
  ImGui::PopStyleColor();
  RenderHealthTestFailureButtons(row.source, &(g_state.*row.enabled),
                                 row.logName);
  ImGui::Unindent();
}

static void RenderSourceGroup(SourceGroup group, const char *groupName) {
  RenderGroupRow(groupName);
  for (int i = 0; i < SOURCE_ROW_COUNT; i++) {
    const SourceRow &row = SOURCE_ROWS[i];
    if (row.group != group)
      continue;
    RenderSourceRow(i);
    if (g_state.*row.enabled && IsFailingHealthTest(row.source))
      RenderHealthTestFailureRow(i);
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
    ImGui::SeparatorText("About the System Sources");
    ImGui::TextWrapped("%s", SYSTEM_SOURCES_INTRO);
    ImGui::Spacing();
    ImGui::TextWrapped("%s", SYSTEM_SOURCES_CREDIT_NOTE);
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
