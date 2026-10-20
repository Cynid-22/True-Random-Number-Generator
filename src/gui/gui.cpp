#include "gui.h"
#include "../core/app_state.h"
#include "../logging/logger.h"
#include "../logic/logic.h"
#include "../logic/output_export.h"
#include "widgets.h"
#include "../platform/secure_clipboard.h"
#include "../platform/unicode_path.h"
#include "output/generation_task.h"
#include "output/one_time_pad_result.h"
#include "output/output_types.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <new>
#include <sstream>
#include <windows.h>
#include <commdlg.h>

void SetupNativeStyle() {
  ImGuiStyle &style = ImGui::GetStyle();

  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.PopupRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.TabRounding = 0.0f;

  style.WindowPadding = ImVec2(8, 8);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 4);
  style.ItemInnerSpacing = ImVec2(4, 4);
  style.IndentSpacing = 20.0f;

  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.TabBorderSize = 1.0f;
  style.SeparatorTextBorderSize = 1.0f;

  style.ScrollbarSize = 14.0f;
  style.GrabMinSize = 10.0f;

  ImVec4 *colors = style.Colors;

  colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

  colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

  colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 0.50f);

  colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

  colors[ImGuiCol_Button] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

  colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

  colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);

  colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

  colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);

  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

  colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
}

static constexpr DWORD SAVE_PATH_MAX_CHARS = 1024;

static bool ShowSaveFileDialog(OPENFILENAMEW &dialog, wchar_t *filePath,
                               const wchar_t *filter,
                               const wchar_t *defaultExtension) {
  ZeroMemory(&dialog, sizeof(dialog));
  dialog.lStructSize = sizeof(dialog);
  dialog.lpstrFilter = filter;
  dialog.lpstrFile = filePath;
  dialog.nMaxFile = SAVE_PATH_MAX_CHARS;
  dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
  dialog.lpstrDefExt = defaultExtension;
  return GetSaveFileNameW(&dialog);
}

static void ShowErrorModal(const char *heading, const std::string &message) {
  g_state.errorModalHeading = heading;
  g_state.errorModalText = message;
  g_state.showErrorModal = true;
}

static const char *const EXPORT_FAILED_HEADING = "Export failed";

static std::string FileNameOfPath(const std::string &path) {
  const size_t separator = path.find_last_of("\\/");
  return separator == std::string::npos ? path : path.substr(separator + 1);
}

void WriteExportFile(const wchar_t *filePath, const char *data, size_t size,
                     const char *exportName) {
  const std::string pathText = Platform::WideToUtf8(filePath);
  if (Platform::WriteWholeFile(filePath, data, size)) {
    g_state.exportStatusText =
        std::string(exportName) + " saved to " + FileNameOfPath(pathText);
    return;
  }
  const unsigned long errorCode = GetLastError();
  g_state.exportStatusText.clear();
  ShowErrorModal(EXPORT_FAILED_HEADING,
                 std::string("Could not save ") + exportName + " to:\n" +
                     pathText + "\n\n" + Platform::SystemErrorText(errorCode) +
                     "\n\nThe file was not saved.");
}

template <typename Byte>
static void WriteAndWipe(const wchar_t *filePath, std::vector<Byte> &content,
                         const char *exportName) {
  try {
    WriteExportFile(filePath, reinterpret_cast<const char *>(content.data()),
                    content.size(), exportName);
  } catch (...) {
    Crypto::SecureClearVector(content);
    throw;
  }
  Crypto::SecureClearVector(content);
}

void WriteAndWipeExportFile(const wchar_t *filePath, std::vector<char> &content,
                            const char *exportName) {
  WriteAndWipe(filePath, content, exportName);
}

void WriteAndWipeExportFile(const wchar_t *filePath,
                            std::vector<uint8_t> &content,
                            const char *exportName) {
  WriteAndWipe(filePath, content, exportName);
}

void ReportExportOutOfMemory(const char *exportName) {
  g_state.exportStatusText.clear();
  ShowErrorModal(EXPORT_FAILED_HEADING,
                 std::string("Not enough memory to export ") + exportName +
                     ".\n\nThe file was not saved.");
}

void CopySecretText(const char *text, size_t length) {
  if (!Platform::CopySecretTextToClipboard(text, length))
    ShowErrorModal("Copy failed",
                   "Could not copy to the clipboard. Another program may be "
                   "using it,\nor the text is too large. Try again, or use "
                   "File > Export Output.");
}

void RenderSecretCopyTooltip() {
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Copied text is kept out of Windows clipboard history "
                      "and cloud sync.\nTRNG clears it from the clipboard "
                      "after %u seconds or when it closes,\nunless something "
                      "else has been copied since.",
                      Platform::SECRET_CLIPBOARD_LIFETIME_SECONDS);
}

void RenderErrorModal() {
  if (g_state.showErrorModal) {
    ImGui::OpenPopup("Error");
    g_state.showErrorModal = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(UiColor::StatusUnavailable, "%s",
                       g_state.errorModalHeading.c_str());
    ImGui::Separator();
    ImGui::TextUnformatted(g_state.errorModalText.c_str());
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0)))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

void ExportWithSaveDialog(const std::wstring &defaultName,
                          const wchar_t *filter,
                          const wchar_t *defaultExtension, const char *data,
                          size_t size, const char *exportName) {
  wchar_t filePath[SAVE_PATH_MAX_CHARS] = {0};
  wcsncpy(filePath, defaultName.c_str(), SAVE_PATH_MAX_CHARS - 1);
  OPENFILENAMEW dialog;
  if (!ShowSaveFileDialog(dialog, filePath, filter, defaultExtension))
    return;
  WriteExportFile(filePath, data, size, exportName);
}

struct OutputFileTypeInfo {
  OutputFileType type;
  const wchar_t *filterName;
  const wchar_t *extension;
};

static constexpr OutputFileTypeInfo OUTPUT_FILE_TYPES[] = {
    {OutputFileType::Text, L"Text Files (*.txt)", L"txt"},
    {OutputFileType::Binary, L"Binary Files (*.bin)", L"bin"},
    {OutputFileType::Csv, L"CSV Files (*.csv)", L"csv"},
    {OutputFileType::Json, L"JSON Files (*.json)", L"json"},
};

static std::wstring BuildOutputFileFilter(size_t typeCount) {
  std::wstring filter;
  for (size_t i = 0; i < typeCount; i++) {
    filter += OUTPUT_FILE_TYPES[i].filterName;
    filter += L'\0';
    filter += L"*.";
    filter += OUTPUT_FILE_TYPES[i].extension;
    filter += L'\0';
  }
  filter += L"All Files (*.*)";
  filter += L'\0';
  filter += L"*.*";
  filter += L'\0';
  return filter;
}

static OutputFileType OutputFileTypeOf(const wchar_t *extension,
                                       DWORD filterIndex, size_t typeCount) {
  for (size_t i = 0; i < typeCount; i++) {
    if (_wcsicmp(extension, OUTPUT_FILE_TYPES[i].extension) == 0)
      return OUTPUT_FILE_TYPES[i].type;
  }
  if (filterIndex >= 1 && filterIndex <= typeCount)
    return OUTPUT_FILE_TYPES[filterIndex - 1].type;
  return OutputFileType::Text;
}

bool ChooseOutputFile(const std::wstring &defaultName, bool textOnly,
                      std::wstring &filePath, OutputFileType &fileType) {
  const size_t typeCount = textOnly ? 1 : std::size(OUTPUT_FILE_TYPES);
  const std::wstring filter = BuildOutputFileFilter(typeCount);
  wchar_t chosenPath[SAVE_PATH_MAX_CHARS] = {0};
  wcsncpy(chosenPath, defaultName.c_str(), SAVE_PATH_MAX_CHARS - 1);
  OPENFILENAMEW dialog;
  if (!ShowSaveFileDialog(dialog, chosenPath, filter.c_str(),
                          OUTPUT_FILE_TYPES[0].extension))
    return false;

  filePath = chosenPath;
  fileType = OutputFileTypeOf(chosenPath + dialog.nFileExtension,
                              dialog.nFilterIndex, typeCount);
  return true;
}

static void ExportSettings() {
  std::ostringstream out;
  out << "TRNG Configuration Export" << std::endl;
  out << "--------------------------" << std::endl;
  out << "Microphone: " << (g_state.microphoneEnabled ? "ON" : "OFF") << std::endl;
  out << "Keystroke: " << (g_state.keystrokeEnabled ? "ON" : "OFF") << std::endl;
  out << "Clock Drift: " << (g_state.clockDriftEnabled ? "ON" : "OFF") << std::endl;
  out << "CPU Jitter: " << (g_state.cpuJitterEnabled ? "ON" : "OFF") << std::endl;
  out << "Mouse Movement: " << (g_state.mouseMovementEnabled ? "ON" : "OFF") << std::endl;
  out << "Target Bits: " << g_state.targetEntropyBits << std::endl;
  out << "Output Format: " << g_state.outputFormat << std::endl;
  const std::string text = out.str();
  ExportWithSaveDialog(L"trng_settings.txt", TEXT_FILE_FILTER, L"txt",
                       text.data(), text.size(), "Settings");
}

static void SaveOutputAs(OutputFileType fileType,
                         const std::wstring &filePath) {
  const std::vector<uint8_t> &binary = g_state.generatedBinaryOutput;
  const char *text = g_state.generatedOutput.data();
  size_t textLength = g_state.generatedOutput.size();
  if (textLength > 0 && g_state.generatedOutput.back() == '\0') {
    textLength--;
  }
  const GeneratedOutputDetails &details = g_state.generatedOutputDetails;

  switch (fileType) {
  case OutputFileType::Text:
    WriteExportFile(filePath.c_str(), text, textLength, "Output");
    break;
  case OutputFileType::Binary:
    WriteExportFile(filePath.c_str(),
                    reinterpret_cast<const char *>(binary.data()),
                    binary.size(), "Output as binary");
    break;
  case OutputFileType::Csv: {
    std::vector<char> csv = BuildOutputCsv(details, text, textLength);
    WriteAndWipeExportFile(filePath.c_str(), csv, "Output as CSV");
    break;
  }
  case OutputFileType::Json: {
    std::vector<char> json =
        BuildOutputJson(details, g_state.generatedAtText, text, textLength);
    WriteAndWipeExportFile(filePath.c_str(), json, "Output as JSON");
    break;
  }
  }
}

static void ExportOutput() {
  if (g_state.hasOneTimePadResult()) {
    ExportOneTimePadKeyAndCiphertext();
    return;
  }
  if (g_state.generatedOutput.empty()) {
    return;
  }

  const bool isGeneratedResult = !g_state.generatedBinaryOutput.empty();
  std::wstring filePath;
  OutputFileType fileType = OutputFileType::Text;
  if (!ChooseOutputFile(L"output.txt", !isGeneratedResult, filePath, fileType))
    return;

  try {
    SaveOutputAs(fileType, filePath);
  } catch (const std::bad_alloc &) {
    ReportExportOutOfMemory("the output");
  }
}

static void ResetSettings() {
  g_state.microphoneEnabled = true;
  g_state.keystrokeEnabled = true;
  g_state.clockDriftEnabled = true;
  g_state.cpuJitterEnabled = true;
  g_state.mouseMovementEnabled = true;
  g_state.decimalDigits = 16;
  g_state.rawDataGroupingEnabled = AppState::DEFAULT_RAW_DATA_GROUPING_ENABLED;
  g_state.rawDataGroupSize = AppState::DEFAULT_RAW_DATA_GROUP_SIZE;
  g_state.rawDataUppercase = AppState::DEFAULT_RAW_DATA_UPPERCASE;
  SelectOutputType(0, 0);
}

void RenderMenuBar() {
  static ULONGLONG s_shortcutExportEndTick = 0;
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) &&
      GetTickCount64() - s_shortcutExportEndTick >= 500) {
    ExportOutput();
    s_shortcutExportEndTick = GetTickCount64();
  }

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Export Settings...")) {
        ExportSettings();
      }
      if (ImGui::MenuItem("Export Output...", "Ctrl+S")) {
        ExportOutput();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        g_state.closeRequested = true;
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Options")) {
      ImGui::MenuItem("Show Used Data Warning", nullptr, &g_state.showUsedDataWarning);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Settings", nullptr, false,
                          !IsGenerationRunning() && !g_state.isCollecting)) {
        ResetSettings();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About TRNG")) {
        g_state.showAboutModal = true;
      }
      ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
  }
}

static void ClearAllEntropyData() {
  g_state.isCollecting = false;

  g_state.entropyPool.SecureWipe();

  g_state.availableEntropyBits = 0.0;
  g_state.usedEntropyBits = 0.0;

  Logger::Log(Logger::Level::INFO, "GUI",
              "All recorded entropy data cleared by user");
}

static void RenderEntropyProgressBar() {
  const float barHeight = 30.0f;

  const float progress = std::min(
      1.0f, static_cast<float>(g_state.availableEntropyBits /
                               g_state.targetEntropyBits));

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

  ImGui::Dummy(ImVec2(0, 0));
  ImVec2 barMin = ImGui::GetCursorScreenPos();
  float barWidth = ImGui::GetContentRegionAvail().x;
  ImVec2 barMax = ImVec2(barMin.x + barWidth, barMin.y + barHeight);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImGuiCol_FrameBg));

  if (progress > 0.0f) {
    ImVec2 progressEnd = ImVec2(barMin.x + barWidth * progress, barMax.y);
    drawList->AddRectFilled(barMin, progressEnd,
                            ImGui::GetColorU32(ImGuiCol_PlotHistogram));
  }

  drawList->AddRect(barMin, barMax, ImGui::GetColorU32(ImGuiCol_Border));

  char overlay[96];
  if (g_state.usedEntropyBits > 0.0) {
    snprintf(overlay, sizeof(overlay),
             "Entropy: %.0f / %.0f bits available (%.0f used)",
             g_state.availableEntropyBits, g_state.targetEntropyBits,
             g_state.usedEntropyBits);
  } else {
    snprintf(overlay, sizeof(overlay), "Entropy: %.0f / %.0f bits",
             g_state.availableEntropyBits, g_state.targetEntropyBits);
  }

  ImVec2 textSize = ImGui::CalcTextSize(overlay);
  ImVec2 textPosition = ImVec2(barMin.x + (barWidth - textSize.x) * 0.5f,
                               barMin.y + (barHeight - textSize.y) * 0.5f);
  drawList->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), overlay);

  ImGui::Dummy(ImVec2(barWidth, barHeight));
  ImGui::PopStyleColor();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Available: entropy credited to new data from the ticked sources.\n"
        "Credit is conservative (at most 1 or 2 bits per timing or "
        "movement, 8 bits per\n64 microphone samples) and stops for a "
        "source that fails an SP 800-90B health test.\n\n"
        "A generation uses up all available entropy. Used data is kept only "
        "as a hash,\nwhich is mixed into every later output but doesn't "
        "count toward the bar again.");
  }
}

static void RenderSecurityMode() {
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Security Mode:");
  ImGui::SameLine();

  if (g_state.hasReachedTargetEntropy()) {
    ImGui::TextColored(UiColor::StatusActive, "ENTROPY-BACKED");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Unused collected entropy (as estimated) covers the bits the "
          "output needs.\nThe output is still the pipeline's ChaCha20 "
          "stream, so security is computational,\nnot "
          "information-theoretic.");
    }
  } else {
    ImGui::TextColored(UiColor::Warning, "EXPANDED");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Unused collected entropy is below the bits the output needs.\nThe "
          "pipeline's ChaCha20 stream stretches the seed to the output size. "
          "\nSecurity is computational. One-Time Pad needs ENTROPY-BACKED.");
    }
  }
}

static void RenderClearConfirmation() {
  if (ImGui::BeginPopupModal("Clear Data Confirmation", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(
        "Warning: You are about to clear %.0f bits of collected entropy.\n\n",
        g_state.entropyPool.GetTotalCreditedBits());
    ImGui::Text("This action cannot be undone.\n\n");
    ImGui::Separator();

    ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
    if (ImGui::Button("Confirm Clear", ImVec2(140, 0))) {
      ClearAllEntropyData();
      ImGui::CloseCurrentPopup();
    }
    ImGui::PopItemFlag();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(140, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

static constexpr double CLEAR_CONFIRMATION_MIN_BITS = 2048.0;

static void RenderCollectionControls() {
  const float clearButtonWidth = 80.0f;
  const float collectionButtonWidth = 160.0f;
  const float buttonsWidth = collectionButtonWidth +
                             ImGui::GetStyle().ItemSpacing.x + clearButtonWidth;

  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0f, ImGui::GetContentRegionAvail().x -
                                          buttonsWidth));
  if (g_state.isCollecting) {
    ImGui::PushStyleColor(ImGuiCol_Button, UiColor::StopButton);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UiColor::StopButtonHovered);
    if (ImGui::Button("Stop Collection", ImVec2(collectionButtonWidth, 0))) {
      g_state.isCollecting = false;
    }
    ImGui::PopStyleColor(2);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.25f, 0.60f, 0.25f, 1.00f));
    if (ImGui::Button("Start Collection", ImVec2(collectionButtonWidth, 0))) {
      g_state.isCollecting = true;
    }
    ImGui::PopStyleColor(2);
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.30f, 0.20f, 1.00f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.60f, 0.35f, 0.25f, 1.00f));
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, g_state.isCollecting);
  if (ImGui::Button("Clear", ImVec2(clearButtonWidth, 0))) {
    if (g_state.entropyPool.GetTotalCreditedBits() >= CLEAR_CONFIRMATION_MIN_BITS) {
      ImGui::OpenPopup("Clear Data Confirmation");
    } else {
      ClearAllEntropyData();
    }
  }
  ImGui::PopItemFlag();
  ImGui::PopStyleColor(2);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear all collected entropy data from all sources.\n"
                      "This will reset all counters to zero.\n"
                      "Collection will be stopped if active.");
  }

  RenderClearConfirmation();
}

void RenderEntropyPoolBar() {
  g_state.availableEntropyBits =
      g_state.entropyPool.GetAvailableBits(g_state.enabledEntropySources());
  g_state.usedEntropyBits = g_state.entropyPool.GetUsedBits();

  RenderEntropyProgressBar();
  RenderSecurityMode();
  RenderCollectionControls();
}

static float StatusBarHeight() { return ImGui::GetFrameHeight(); }

float StatusBarReservedHeight() {
  const ImGuiStyle &style = ImGui::GetStyle();
  return std::max(0.0f, StatusBarHeight() + style.ItemSpacing.y -
                            style.WindowPadding.y);
}

struct StatusBarWriter {
  ImDrawList *drawList;
  ImVec2 textPosition;
  float top;
  float bottom;

  void Text(const ImVec4 &color, const char *text) {
    drawList->AddText(textPosition, ImGui::GetColorU32(color), text);
    textPosition.x += ImGui::CalcTextSize(text).x;
  }

  void Divider() {
    const ImGuiStyle &style = ImGui::GetStyle();
    textPosition.x += style.WindowPadding.x;
    const float lineX = std::floor(textPosition.x) + 0.5f;
    drawList->AddLine(ImVec2(lineX, top + style.FramePadding.y),
                      ImVec2(lineX, bottom - style.FramePadding.y),
                      ImGui::GetColorU32(ImGuiCol_Separator));
    textPosition.x += style.WindowPadding.x;
  }
};

void RenderStatusBar() {
  const ImVec2 windowPos = ImGui::GetWindowPos();
  const ImVec2 windowSize = ImGui::GetWindowSize();
  const float barHeight = StatusBarHeight();
  const ImVec2 barMin(windowPos.x, windowPos.y + windowSize.y - barHeight);
  const ImVec2 barMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->PushClipRect(barMin, barMax, false);
  drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
  drawList->AddLine(barMin, ImVec2(barMax.x, barMin.y),
                    ImGui::GetColorU32(ImGuiCol_Border));

  StatusBarWriter status{
      drawList,
      ImVec2(barMin.x + ImGui::GetStyle().WindowPadding.x,
             barMin.y + (barHeight - ImGui::GetTextLineHeight()) * 0.5f),
      barMin.y, barMax.y};

  if (g_state.isCollecting)
    status.Text(UiColor::StatusActive, "Collecting...");
  else
    status.Text(UiColor::DimText, "Stopped");

  char text[128];
  snprintf(text, sizeof(text), "%d of %d sources included",
           static_cast<int>(g_state.enabledEntropySources().size()),
           static_cast<int>(Entropy::ENTROPY_SOURCE_COUNT));
  status.Divider();
  status.Text(UiColor::LabelText, text);

  if (IsGenerationRunning()) {
    snprintf(text, sizeof(text), "Generating output...  %s",
             CurrentSpinnerFrame());
    status.Divider();
    status.Text(UiColor::InProgress, text);
  } else if (!g_state.generatedAtText.empty()) {
    snprintf(text, sizeof(text), "Output generated at: %s",
             g_state.generatedAtText.c_str());
    status.Divider();
    status.Text(UiColor::LabelText, text);
  }

  if (!g_state.exportStatusText.empty()) {
    status.Divider();
    status.Text(UiColor::LabelText, g_state.exportStatusText.c_str());
  }

  const int clipboardClearSeconds = Platform::SecondsUntilSecretClipboardClear();
  if (clipboardClearSeconds >= 0) {
    snprintf(text, sizeof(text), "Clipboard clears in %d s",
             clipboardClearSeconds);
    status.Divider();
    status.Text(UiColor::LabelText, text);
  }

  drawList->PopClipRect();
}

void RenderAboutModal() {
  if (g_state.showAboutModal) {
    ImGui::OpenPopup("About TRNG");
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("About TRNG", &g_state.showAboutModal, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("True Random Number Generator (TRNG)");
    ImGui::Text("Version %s", APP_VERSION);
    ImGui::Separator();
    ImGui::Text("A high-security cryptographic randomness tool.");
    ImGui::Text("Uses a Quad-Layer CSPRNG architecture:");
    ImGui::BulletText("Layer 1: ChaCha20 Masking");
    ImGui::BulletText("Layer 2: Entropy Injection (XOR Fold)");
    ImGui::BulletText("Layer 3: AES-256 Transformation");
    ImGui::BulletText("Layer 4: ChaCha20 Final Whitening");
    ImGui::Text("Every output, in both security modes, is this pipeline's "
                "ChaCha20 stream");
    ImGui::Text("seeded from the collected entropy. Security is "
                "computational.");
    ImGui::Spacing();
    ImGui::Text("Entropy sources: Clock Drift, CPU Jitter, Keystrokes, Mouse, Mic.");
    ImGui::Separator();

    if (ImGui::Button("Close", ImVec2(120, 0))) {
      g_state.showAboutModal = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void RenderClosingModal() {
  if (!g_state.closeRequested || !IsGenerationRunning())
    return;

  g_state.cancelGenerationRequested = true;
  if (!ImGui::IsPopupOpen("Closing TRNG")) {
    ImGui::OpenPopup("Closing TRNG");
    g_state.closeRequestedTime = ImGui::GetTime();
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Closing TRNG", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoMove)) {
    ImGui::Text("Stopping the generation in progress.");
    ImGui::Text("The app will close as soon as it has stopped. The unfinished "
                "output is discarded and all data is wiped.");
    if (g_state.generationInCryptoPass) {
      ImGui::Spacing();
      ImGui::TextColored(UiColor::Warning,
                         "The encryption pass is running and can't be "
                         "interrupted. For very large outputs this can take a "
                         "while.");
    }
    ImGui::Spacing();
    ImGui::TextColored(UiColor::DimText, "Waiting: %.1fs  %s",
                       ImGui::GetTime() - g_state.closeRequestedTime,
                       CurrentSpinnerFrame());
    ImGui::EndPopup();
  }
}
