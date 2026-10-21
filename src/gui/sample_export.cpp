#include "sample_export.h"
#include "../core/app_state.h"
#include "../entropy/sample_recorder.h"
#include "../platform/unicode_path.h"
#include "imgui.h"
#include "widgets.h"
#include <algorithm>
#include <windows.h>
#include <shobjidl.h>

using Entropy::EntropySource;
using Entropy::SampleRecorder;

namespace {

constexpr ULONGLONG RESTART_ROW_TIMEOUT_MS = 15000;
constexpr const char *RESTART_POPUP_TITLE = "Restart Test";
constexpr float RESTART_PROGRESS_WIDTH = 360.0f;
constexpr float DIALOG_BUTTON_WIDTH = 120.0f;

constexpr const char *SOURCE_NAMES[Entropy::ENTROPY_SOURCE_COUNT] = {
    "Microphone", "Keystroke", "Clock Drift", "CPU Jitter", "Mouse"};

constexpr EntropySource RESTART_TEST_SOURCES[] = {
    EntropySource::ClockDrift, EntropySource::CpuJitter,
    EntropySource::Microphone};
constexpr const char *RESTART_TEST_SOURCE_NAMES[] = {"Clock Drift", "CPU Jitter",
                                                     "Microphone"};

SampleRecorder s_recorder;
bool s_windowOpen = false;
bool s_recordingSession = false;
bool s_openRestartPopup = false;
std::wstring s_folder;
int s_restartSourceChoice = 0;
ULONGLONG s_rowStartTick = 0;
std::string s_resultText;
bool s_resultIsError = false;

const char *SourceName(EntropySource source) {
  return SOURCE_NAMES[static_cast<size_t>(source)];
}

std::array<bool, Entropy::ENTROPY_SOURCE_COUNT> EnabledSources() {
  std::array<bool, Entropy::ENTROPY_SOURCE_COUNT> enabled{};
  enabled[static_cast<size_t>(EntropySource::Microphone)] = g_state.microphoneEnabled;
  enabled[static_cast<size_t>(EntropySource::Keystroke)] = g_state.keystrokeEnabled;
  enabled[static_cast<size_t>(EntropySource::ClockDrift)] = g_state.clockDriftEnabled;
  enabled[static_cast<size_t>(EntropySource::CpuJitter)] = g_state.cpuJitterEnabled;
  enabled[static_cast<size_t>(EntropySource::Mouse)] = g_state.mouseMovementEnabled;
  return enabled;
}

bool AnySourceEnabled() {
  const auto enabled = EnabledSources();
  return std::any_of(enabled.begin(), enabled.end(),
                     [](bool sourceEnabled) { return sourceEnabled; });
}

template <typename Action> void WithCollector(EntropySource source, Action action) {
  switch (source) {
  case EntropySource::Microphone: action(g_state.microphoneCollector); break;
  case EntropySource::Keystroke: action(g_state.keystrokeCollector); break;
  case EntropySource::ClockDrift: action(g_state.clockDriftCollector); break;
  case EntropySource::CpuJitter: action(g_state.cpuJitterCollector); break;
  case EntropySource::Mouse: action(g_state.mouseCollector); break;
  }
}

void ShowResult(const std::string &text, bool isError) {
  s_resultText = text;
  s_resultIsError = isError;
}

std::string FolderText() { return Platform::WideToUtf8(s_folder.c_str()); }

std::string RecordedCountsText() {
  std::string text;
  const auto enabled = EnabledSources();
  for (size_t sourceIndex = 0; sourceIndex < Entropy::ENTROPY_SOURCE_COUNT; sourceIndex++) {
    if (!enabled[sourceIndex])
      continue;
    const EntropySource source = static_cast<EntropySource>(sourceIndex);
    text += std::string("\n  ") + SampleRecorder::FileStem(source) + ".u64: " +
            WithThousandsSeparators(s_recorder.SamplesWritten(source)) + " values";
  }
  return text;
}

bool ChooseFolder(std::wstring &folder) {
  const HRESULT comResult =
      CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  bool chosen = false;
  IFileOpenDialog *dialog = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&dialog)))) {
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    IShellItem *item = nullptr;
    if (SUCCEEDED(dialog->Show(GetActiveWindow())) &&
        SUCCEEDED(dialog->GetResult(&item))) {
      PWSTR path = nullptr;
      if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        folder = path;
        CoTaskMemFree(path);
        chosen = true;
      }
      item->Release();
    }
    dialog->Release();
  }
  if (SUCCEEDED(comResult))
    CoUninitialize();
  return chosen;
}

void StartRecording() {
  if (!s_recorder.BeginRecording(s_folder, EnabledSources())) {
    ShowResult(s_recorder.LastError(), true);
    return;
  }
  ShowResult("", false);
  s_recordingSession = true;
  g_state.isCollecting = true;
}

void StopRecording(const char *reason) {
  g_state.isCollecting = false;
  s_recorder.Finish();
  ShowResult(std::string(reason) + " Saved to " + FolderText() + ":" +
                 RecordedCountsText(),
             false);
}

void StopRestartCollector() {
  WithCollector(s_recorder.RestartSource(), [](auto &collector) {
    collector.Stop();
    collector.Harvest();
  });
}

void EndRestartTest(const std::string &text, bool isError) {
  StopRestartCollector();
  s_recorder.Finish();
  ShowResult(text, isError);
}

void StartRestartTest() {
  const EntropySource source = RESTART_TEST_SOURCES[s_restartSourceChoice];
  if (!s_recorder.BeginRestartTest(s_folder, source)) {
    ShowResult(s_recorder.LastError(), true);
    return;
  }
  ShowResult("", false);
  s_rowStartTick = GetTickCount64();
  s_openRestartPopup = true;
}

void RunRestartTestFrame() {
  const EntropySource source = s_recorder.RestartSource();
  bool collectorStopped = false;
  SampleRecorder::RowStatus status = SampleRecorder::RowStatus::Filling;

  WithCollector(source, [&](auto &collector) {
    if (!collector.IsRunning()) {
      if (s_recorder.RestartRowStarted())
        collectorStopped = true;
      else
        collector.Start();
      return;
    }
    status = s_recorder.AddToRestartRow(collector.Harvest());
    if (status == SampleRecorder::RowStatus::Complete) {
      collector.Stop();
      collector.Harvest();
    }
  });

  const std::string restartText =
      std::to_string(s_recorder.RestartsCompleted()) + " of " +
      std::to_string(SampleRecorder::RESTART_COUNT) + " restarts";

  if (status == SampleRecorder::RowStatus::Failed) {
    EndRestartTest(s_recorder.LastError(), true);
  } else if (collectorStopped) {
    EndRestartTest(std::string(SourceName(source)) + " stopped by itself after " +
                       restartText + ". The file is incomplete.",
                   true);
  } else if (status == SampleRecorder::RowStatus::Complete) {
    s_rowStartTick = GetTickCount64();
    if (s_recorder.RestartTestComplete())
      ShowResult("Restart test finished: " + restartText + " saved to " +
                     FolderText() + "\\" + SampleRecorder::FileStem(source) +
                     "_restart.u64",
                 false);
  } else if (GetTickCount64() - s_rowStartTick > RESTART_ROW_TIMEOUT_MS) {
    EndRestartTest(std::string("No samples from ") + SourceName(source) +
                       " for 15 seconds after " + restartText +
                       ". The file is incomplete.",
                   true);
  }
}

void RenderRestartPopup() {
  if (s_openRestartPopup) {
    ImGui::OpenPopup(RESTART_POPUP_TITLE);
    s_openRestartPopup = false;
  }

  const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (!ImGui::BeginPopupModal(RESTART_POPUP_TITLE, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    return;

  if (!s_recorder.IsRestartTest()) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }

  const size_t completed = s_recorder.RestartsCompleted();
  ImGui::Text("%s: %d of %d restarts", SourceName(s_recorder.RestartSource()),
              static_cast<int>(completed),
              static_cast<int>(SampleRecorder::RESTART_COUNT));
  ImGui::ProgressBar(static_cast<float>(completed) /
                         static_cast<float>(SampleRecorder::RESTART_COUNT),
                     ImVec2(RESTART_PROGRESS_WIDTH, 0.0f));
  ImGui::TextDisabled("Keep TRNG open until it finishes.");
  ImGui::Spacing();
  if (ImGui::Button("Cancel", ImVec2(DIALOG_BUTTON_WIDTH, 0.0f)))
    EndRestartTest("Restart test cancelled after " + std::to_string(completed) +
                       " restarts. The file is incomplete.",
                   true);
  ImGui::EndPopup();
}

void RenderFolderRow(bool busy) {
  ImGui::BeginDisabled(busy);
  if (ImGui::Button("Choose Folder..."))
    ChooseFolder(s_folder);
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (s_folder.empty())
    ImGui::TextDisabled("No folder chosen");
  else
    ImGui::TextUnformatted(FolderText().c_str());
}

void RenderRecordSection(bool busy) {
  ImGui::SeparatorText("Record");
  ImGui::TextWrapped("Records the sources ticked on the Input tab while collecting. "
                     "Type, move the mouse and talk as usual. Each run adds to "
                     "<source>.u64.");

  if (s_recorder.IsRecording()) {
    ImGui::TextColored(UiColor::StatusActive, "Recording...%s",
                       RecordedCountsText().c_str());
    if (ImGui::Button("Stop Recording"))
      StopRecording("Recording stopped.");
    return;
  }

  const bool canStart = !busy && !s_folder.empty() && !g_state.isCollecting &&
                        AnySourceEnabled();
  ImGui::BeginDisabled(!canStart);
  if (ImGui::Button("Start Recording"))
    StartRecording();
  ImGui::EndDisabled();
  if (g_state.isCollecting)
    ImGui::TextDisabled("Stop collection first.");
  else if (!AnySourceEnabled())
    ImGui::TextDisabled("Tick at least one source on the Input tab.");
}

void RenderRestartSection(bool busy) {
  ImGui::SeparatorText("Restart test");
  ImGui::TextWrapped("Stops and starts one source 1,000 times and keeps the first "
                     "1,000 values after each start (16 values, 1,024 LSBs, for the "
                     "microphone) in <source>_restart.u64, replacing an earlier file.");

  ImGui::BeginDisabled(busy || g_state.isCollecting);
  ImGui::SetNextItemWidth(ComboWidthForText(ImGui::CalcTextSize("Clock Drift").x));
  ImGui::Combo("Source", &s_restartSourceChoice, RESTART_TEST_SOURCE_NAMES,
               IM_ARRAYSIZE(RESTART_TEST_SOURCE_NAMES));
  ImGui::SameLine();
  ImGui::BeginDisabled(s_folder.empty());
  if (ImGui::Button("Run Restart Test"))
    StartRestartTest();
  ImGui::EndDisabled();
  ImGui::EndDisabled();
}

}

void RenderSampleExportMenu() {
  if (ImGui::BeginMenu("Tools")) {
    ImGui::MenuItem("Sample Export...", nullptr, &s_windowOpen);
    ImGui::EndMenu();
  }
}

void RenderSampleExportWindow() {
  RenderRestartPopup();
  if (!s_windowOpen)
    return;

  ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Sample Export", &s_windowOpen)) {
    ImGui::End();
    return;
  }

  ImGui::PushStyleColor(ImGuiCol_Text, UiColor::Warning);
  ImGui::TextWrapped("Test build. Writes each source's raw values (little-endian "
                     "uint64, before any hashing) for NIST SP 800-90B. Recorded data "
                     "never reaches the entropy pool. The files are real entropy: use "
                     "them only for testing and delete them afterwards.");
  ImGui::PopStyleColor();
  ImGui::Spacing();

  const bool busy = s_recorder.IsRecording() || s_recorder.IsRestartTest();
  RenderFolderRow(busy);
  RenderRecordSection(busy);
  RenderRestartSection(busy);

  if (!s_resultText.empty()) {
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, s_resultIsError ? UiColor::StatusUnavailable
                                                         : UiColor::LabelText);
    ImGui::TextWrapped("%s", s_resultText.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::End();
}

bool UpdateSampleExport() {
  if (s_recordingSession && !g_state.isCollecting) {
    if (s_recorder.IsRecording())
      StopRecording("Collection stopped, recording ended.");
    s_recordingSession = false;
  }
  if (!s_recorder.IsRestartTest())
    return false;
  RunRestartTestFrame();
  return true;
}

bool RecordSampleExportData(const Entropy::EntropyDataVector &data) {
  if (!s_recordingSession)
    return false;
  if (s_recorder.IsRecording() && !s_recorder.Record(data)) {
    g_state.isCollecting = false;
    ShowResult(s_recorder.LastError() + "\nRecording stopped.", true);
  }
  return true;
}
