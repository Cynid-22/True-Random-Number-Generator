#include "gui.h"
#include "../core/app_state.h"
#include "../logging/logger.h"
#include "../logic/logic.h"
#include "widgets.h"
#include "output/generation_task.h"
#include "output/output_types.h"
#include "imgui.h"
#include <cstdio>
#include <set>
#include <windows.h>
#include <commdlg.h>
#include <fstream>

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

static bool ShowSaveFileDialog(char *filePath, const char *filter) {
  OPENFILENAMEA dialog;
  ZeroMemory(&dialog, sizeof(dialog));
  dialog.lStructSize = sizeof(dialog);
  dialog.lpstrFilter = filter;
  dialog.lpstrFile = filePath;
  dialog.nMaxFile = MAX_PATH;
  dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
  dialog.lpstrDefExt = "txt";
  return GetSaveFileNameA(&dialog);
}

static void ExportSettings() {
  char filePath[MAX_PATH] = "trng_settings.txt";
  if (!ShowSaveFileDialog(filePath, "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0"))
    return;

  std::ofstream out(filePath);
  if (out.is_open()) {
    out << "TRNG Configuration Export" << std::endl;
    out << "--------------------------" << std::endl;
    out << "Microphone: " << (g_state.microphoneEnabled ? "ON" : "OFF") << std::endl;
    out << "Keystroke: " << (g_state.keystrokeEnabled ? "ON" : "OFF") << std::endl;
    out << "Clock Drift: " << (g_state.clockDriftEnabled ? "ON" : "OFF") << std::endl;
    out << "CPU Jitter: " << (g_state.cpuJitterEnabled ? "ON" : "OFF") << std::endl;
    out << "Mouse Movement: " << (g_state.mouseMovementEnabled ? "ON" : "OFF") << std::endl;
    out << "Target Bits: " << g_state.targetEntropyBits << std::endl;
    out << "Output Format: " << g_state.outputFormat << std::endl;
    out.close();
    Logger::Log(Logger::Level::INFO, "GUI", "Settings exported to %s", filePath);
  }
}

static void ExportOutput() {
  if (g_state.generatedOutput.empty()) {
    return;
  }

  char filePath[MAX_PATH] = "output.txt";
  if (!ShowSaveFileDialog(filePath, "Text Files (*.txt)\0*.txt\0Binary Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0"))
    return;

  std::ofstream out(filePath, std::ios::binary);
  if (out.is_open()) {
    size_t size = g_state.generatedOutput.size();
    if (size > 0 && g_state.generatedOutput.back() == '\0') {
      size--;
    }
    out.write(g_state.generatedOutput.data(), size);
    out.close();
    Logger::Log(Logger::Level::INFO, "GUI", "Output exported to %s", filePath);
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
  if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S)) {
    ExportOutput();
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
      ImGui::MenuItem("Auto-start collection", nullptr, &g_state.isCollecting);
      ImGui::MenuItem("Show Locked Data Warning", nullptr, &g_state.showDataLockWarning);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Settings", nullptr, false, !IsGenerationRunning())) {
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

  (void)g_state.clockDriftCollector.Harvest();

  g_state.entropyPool.SecureWipe();

  g_state.microphoneEntropyBits = 0.0f;
  g_state.keystrokeEntropyBits = 0.0f;
  g_state.clockDriftEntropyBits = 0.0f;
  g_state.cpuJitterEntropyBits = 0.0f;
  g_state.mouseEntropyBits = 0.0f;
  g_state.collectedEntropyBits = 0.0f;
  g_state.lockedDataTimestamp = 0;

  Logger::Log(Logger::Level::INFO, "GUI",
              "All recorded entropy data cleared by user");
}

static std::set<Entropy::EntropySource> GetEnabledEntropySources() {
  std::set<Entropy::EntropySource> enabledSources;
  if (g_state.microphoneEnabled)
    enabledSources.insert(Entropy::EntropySource::Microphone);
  if (g_state.keystrokeEnabled)
    enabledSources.insert(Entropy::EntropySource::Keystroke);
  if (g_state.clockDriftEnabled)
    enabledSources.insert(Entropy::EntropySource::ClockDrift);
  if (g_state.cpuJitterEnabled)
    enabledSources.insert(Entropy::EntropySource::CpuJitter);
  if (g_state.mouseMovementEnabled)
    enabledSources.insert(Entropy::EntropySource::Mouse);
  return enabledSources;
}

static void RenderEntropyProgressBar(float lockedBits, float newBits) {
  const float barHeight = 30.0f;

  float totalProgress = g_state.collectedEntropyBits / g_state.targetEntropyBits;
  if (totalProgress > 1.0f)
    totalProgress = 1.0f;

  float lockedProgress = lockedBits / g_state.targetEntropyBits;
  if (lockedProgress > 1.0f)
    lockedProgress = 1.0f;

  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

  ImGui::Dummy(ImVec2(0, 0));
  ImVec2 barMin = ImGui::GetCursorScreenPos();
  float barWidth = ImGui::GetContentRegionAvail().x;
  ImVec2 barMax = ImVec2(barMin.x + barWidth, barMin.y + barHeight);

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImGuiCol_FrameBg));

  if (lockedProgress > 0.0f) {
    ImVec2 lockedMax = ImVec2(barMin.x + barWidth * lockedProgress, barMax.y);
    drawList->AddRectFilled(barMin, lockedMax, IM_COL32(40, 100, 160, 255));
  }

  if (totalProgress > lockedProgress) {
    ImVec2 newStart = ImVec2(barMin.x + barWidth * lockedProgress, barMin.y);
    ImVec2 newEnd = ImVec2(barMin.x + barWidth * totalProgress, barMax.y);
    drawList->AddRectFilled(newStart, newEnd,
                            ImGui::GetColorU32(ImGuiCol_PlotHistogram));
  }

  drawList->AddRect(barMin, barMax, ImGui::GetColorU32(ImGuiCol_Border));

  char overlay[64];
  if (g_state.lockedDataTimestamp > 0 && lockedBits > 0) {
    snprintf(overlay, sizeof(overlay),
             "Entropy: %.0f (Locked) + %.0f (New) / %.0f", lockedBits, newBits,
             g_state.targetEntropyBits);
  } else {
    snprintf(overlay, sizeof(overlay), "Entropy: %.0f / %.0f bits",
             g_state.collectedEntropyBits, g_state.targetEntropyBits);
  }

  ImVec2 textSize = ImGui::CalcTextSize(overlay);
  ImVec2 textPosition = ImVec2(barMin.x + (barWidth - textSize.x) * 0.5f,
                               barMin.y + (barHeight - textSize.y) * 0.5f);
  drawList->AddText(textPosition, ImGui::GetColorU32(ImGuiCol_Text), overlay);

  ImGui::Dummy(ImVec2(barWidth, barHeight));
  ImGui::PopStyleColor();
}

static void RenderSecurityMode() {
  ImGui::Text("Security Mode:");
  ImGui::SameLine();

  if (g_state.hasReachedTargetEntropy()) {
    ImGui::TextColored(UiColor::StatusActive,
                       "TRUE RANDOMNESS (Consolidation)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Input Entropy >= Target Output. \nWe will condense "
                        "raw data into perfect random bits. \nInformation "
                        "Theoretic Security possible (for OTP).");
    }
  } else {
    ImGui::TextColored(UiColor::Warning,
                       "PSEUDO-RANDOM (Expansion)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Input Entropy < Target Output. \nWe must use a CSPRNG to expand the "
          "key. \nComputationally Secure, but not 'True' Random for OTP.");
    }
  }
}

static void RenderClearConfirmation() {
  if (ImGui::BeginPopupModal("Clear Data Confirmation", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(
        "Warning: You are about to clear %.0f bits of collected entropy.\n\n",
        g_state.collectedEntropyBits);
    ImGui::Text("This action cannot be undone.\n\n");
    ImGui::Separator();

    if (ImGui::Button("Confirm Clear", ImVec2(140, 0))) {
      ClearAllEntropyData();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(140, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

static void RenderCollectionControls() {
  const float rightMargin = 24.0f;
  const float clearButtonWidth = 80.0f;
  const float collectionButtonWidth = 160.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float buttonStartX = ImGui::GetWindowWidth() - rightMargin -
                             clearButtonWidth - spacing - collectionButtonWidth;
  const float statusEndX = buttonStartX - spacing;

  ImGui::SameLine(buttonStartX - 120.0f);
  const char *statusText =
      g_state.isCollecting ? "[Collecting...]" : "[Stopped]";
  ImVec2 statusSize = ImGui::CalcTextSize(statusText);
  ImGui::SetCursorPosX(statusEndX - statusSize.x);
  if (g_state.isCollecting) {
    ImGui::TextColored(UiColor::StatusActive, "%s", statusText);
  } else {
    ImGui::TextColored(UiColor::DimText, "%s", statusText);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", g_state.isCollecting ? "Collection in progress."
                                                 : "Collection stopped.");
  }

  ImGui::SameLine(buttonStartX);
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
  if (ImGui::Button("Clear", ImVec2(clearButtonWidth, 0))) {
    if (g_state.collectedEntropyBits >= 2048.0f) {
      ImGui::OpenPopup("Clear Data Confirmation");
    } else {
      ClearAllEntropyData();
    }
  }
  ImGui::PopStyleColor(2);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear all collected entropy data from all sources.\n"
                      "This will reset all counters to zero.\n"
                      "Collection will be stopped if active.");
  }

  RenderClearConfirmation();
}

void RenderEntropyPoolBar() {
  float lockedBits =
      g_state.entropyPool.GetEntropyBitsBefore(g_state.lockedDataTimestamp);
  float newBits = g_state.entropyPool.GetEntropyBitsAfter(
      g_state.lockedDataTimestamp, GetEnabledEntropySources());

  g_state.collectedEntropyBits = lockedBits + newBits;

  RenderEntropyProgressBar(lockedBits, newBits);
  RenderSecurityMode();
  RenderCollectionControls();
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
