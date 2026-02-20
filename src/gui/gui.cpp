// TRNG - GUI Implementation
// Contains core GUI rendering functions: Style, Menu, Layout, Common widgets

#include "gui.h"
#include "../core/app_state.h"
#include "../logging/logger.h"
#include "../logic/logic.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <cstdio>
#include <d3d11.h>
#include <thread>
#include <commdlg.h>
#include <fstream>
#include "../logic/csprng.h" // For GenerateNistData

//=============================================================================
// STYLE SETUP
//=============================================================================

void SetupNativeStyle() {
  ImGuiStyle &style = ImGui::GetStyle();

  // Minimal rounding for native look
  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.PopupRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;
  style.TabRounding = 0.0f;

  // Tight padding like native apps
  style.WindowPadding = ImVec2(8, 8);
  style.FramePadding = ImVec2(6, 4);
  style.ItemSpacing = ImVec2(8, 4);
  style.ItemInnerSpacing = ImVec2(4, 4);
  style.IndentSpacing = 20.0f;

  // Borders
  style.WindowBorderSize = 1.0f;
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.PopupBorderSize = 1.0f;
  style.TabBorderSize = 1.0f;

  // Scrollbar
  style.ScrollbarSize = 14.0f;
  style.GrabMinSize = 10.0f;

  // Native dark theme colors (like WizTree)
  ImVec4 *colors = style.Colors;

  // Window backgrounds - dark grays
  colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

  // Borders - subtle
  colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  // Frame backgrounds
  colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

  // Title bar
  colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.14f, 0.50f);

  // Menu bar
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

  // Buttons - blue accent like Windows
  colors[ImGuiCol_Button] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

  // Headers
  colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

  // Checkbox/Radio - blue accent
  colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);

  // Slider
  colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);

  // Scrollbar
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

  // Progress bar - blue
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.50f, 0.80f, 1.00f, 1.00f);

  // Text - crisp white/gray
  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

  // Separator
  colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

  // Table
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
}

//=============================================================================
// MENU BAR
//=============================================================================

void RenderMenuBar() {
  // Handle Global Shortcuts
  if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_S)) {
    HandleExportOutput();
  }

  if (ImGui::BeginMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Export Settings...")) {
        char filename[MAX_PATH] = "trng_settings.txt";
        OPENFILENAMEA ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = "txt";

        if (GetSaveFileNameA(&ofn)) {
          std::ofstream out(filename);
          if (out.is_open()) {
            out << "TRNG Configuration Export" << std::endl;
            out << "--------------------------" << std::endl;
            out << "Microphone: " << (g_state.microphoneEnabled ? "ON" : "OFF") << std::endl;
            out << "Keystroke: " << (g_state.keystrokeEnabled ? "ON" : "OFF") << std::endl;
            out << "Clock Drift: " << (g_state.clockDriftEnabled ? "ON" : "OFF") << std::endl;
            out << "CPU Jitter: " << (g_state.cpuJitterEnabled ? "ON" : "OFF") << std::endl;
            out << "Mouse Movement: " << (g_state.mouseMovementEnabled ? "ON" : "OFF") << std::endl;
            out << "Target Bits: " << g_state.targetBits << std::endl;
            out << "Output Format: " << g_state.outputFormat << std::endl;
            out.close();
            Logger::Log(Logger::Level::INFO, "GUI", "Settings exported to %s", filename);
          }
        }
      }
      if (ImGui::MenuItem("Export Output...", "Ctrl+S")) {
        HandleExportOutput();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        PostQuitMessage(0);
      }
      ImGui::EndMenu();
    }


    if (ImGui::BeginMenu("Options")) {
      ImGui::MenuItem("Auto-start collection", nullptr, &g_state.isCollecting);
      ImGui::MenuItem("Show Locked Data Warning", nullptr, &g_state.showDataLockWarning);
      ImGui::Separator();
      if (ImGui::MenuItem("Reset Settings")) {
        // Reset configuration (can't assign AppState due to mutex/atomics)
        g_state.microphoneEnabled = true;
        g_state.keystrokeEnabled = true;
        g_state.clockDriftEnabled = true;
        g_state.cpuJitterEnabled = true;
        g_state.mouseMovementEnabled = true;
        g_state.outputFormat = 0;
        g_state.decimalDigits = 16;
        // ... reset other fields as needed
      }
      ImGui::EndMenu();
    }
    // Debug menu removed for security hardening (Issue 5 from security audit)
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About TRNG")) {
        g_state.showAboutModal = true;
      }
      ImGui::EndMenu();
    }

    // Global Status if Logging is ON
    if (g_state.keepLogs) {
      const char* logPath = Logger::GetCurrentLogPath();
      char statusText[512];
      snprintf(statusText, sizeof(statusText), "[LOGGING ON: %s]", logPath);

      // Calc size to right align
      ImVec2 textSize = ImGui::CalcTextSize(statusText);
      ImGui::SameLine(ImGui::GetWindowWidth() - textSize.x - 20); // 20px padding

      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "%s", statusText);
    }

    ImGui::EndMenuBar();
  }
}

//=============================================================================
// HELPER FUNCTIONS
//=============================================================================

static void ClearAllEntropyData() {
  // Stop collection if active
  if (g_state.isCollecting) {
    g_state.isCollecting = false;
  }

  // Drain clock drift collector buffer so no residual data remains
  (void)g_state.clockDriftCollector.Harvest();

  // SECURITY: Securely wipe all pooled entropy data
  g_state.entropyPool.SecureWipe();

  // Clear all entropy data counters
  g_state.entropyMic = 0.0f;
  g_state.entropyKeystroke = 0.0f;
  g_state.entropyClock = 0.0f;
  g_state.entropyJitter = 0.0f;
  g_state.entropyMouse = 0.0f;
  g_state.collectedBits = 0.0f;
  g_state.lockedDataTimestamp = 0; // Reset lock

  // From this point onwards, treat session as clean: don't show logging warning
  // unless the user enables logging again after the clear
  g_state.loggingWasEverEnabled = false;
  g_state.showLoggingWarningWindow = false;

  Logger::Log(Logger::Level::INFO, "GUI",
              "All recorded entropy data cleared by user");
}

//=============================================================================
// ENTROPY POOL BAR (with Start/Stop Button)
//=============================================================================

//=============================================================================
// ENTROPY POOL BAR
//=============================================================================

void RenderEntropyPoolBar() {
  // Build set of currently enabled sources
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

  // Calculate split entropy for visualization
  float lockedBits =
      g_state.entropyPool.GetEntropyBitsBefore(g_state.lockedDataTimestamp);
  float newBits = g_state.entropyPool.GetEntropyBitsAfter(
      g_state.lockedDataTimestamp, enabledSources);

  // Total is sum of both
  g_state.collectedBits = lockedBits + newBits;

  // Progress bar visualization
  float progressTotal = g_state.collectedBits / g_state.targetBits;
  if (progressTotal > 1.0f)
    progressTotal = 1.0f;

  float progressLocked = lockedBits / g_state.targetBits;
  if (progressLocked > 1.0f)
    progressLocked = 1.0f;

  // Custom Progress Bar with two segments
  ImVec2 barSize = ImVec2(-1, 30);
  ImGui::PushStyleColor(
      ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f)); // Ensure text is visible

  // Reserve space for the progress bar
  ImGui::Dummy(ImVec2(0, 0)); // Hack to align properly? No, just use CursorPos
  ImVec2 p_min = ImGui::GetCursorScreenPos();
  float width = ImGui::GetContentRegionAvail().x;
  ImVec2 p_max = ImVec2(p_min.x + width, p_min.y + 30);

  // Draw background
  ImGui::GetWindowDrawList()->AddRectFilled(
      p_min, p_max, ImGui::GetColorU32(ImGuiCol_FrameBg));

  // Draw Locked Segment (Darker color)
  if (progressLocked > 0.0f) {
    ImVec2 p_locked_max = ImVec2(p_min.x + width * progressLocked, p_max.y);
    // Use a darker "Locked" color (e.g., Dark Green/Blue)
    ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_locked_max,
                                              IM_COL32(40, 100, 160, 255));
  }

  // Draw New Segment (Standard color) - starts where locked ends
  if (progressTotal > progressLocked) {
    ImVec2 p_new_start = ImVec2(p_min.x + width * progressLocked, p_min.y);
    ImVec2 p_new_end = ImVec2(p_min.x + width * progressTotal, p_max.y);
    // Standard "Active" color (e.g., Bright Blue)
    ImGui::GetWindowDrawList()->AddRectFilled(
        p_new_start, p_new_end, ImGui::GetColorU32(ImGuiCol_PlotHistogram));
  }

  // Draw Border
  ImGui::GetWindowDrawList()->AddRect(p_min, p_max,
                                      ImGui::GetColorU32(ImGuiCol_Border));

  // Render Text Overlay (Centered)
  char overlay[64];
  if (g_state.lockedDataTimestamp > 0 && lockedBits > 0) {
    snprintf(overlay, sizeof(overlay),
             "Entropy: %.0f (Locked) + %.0f (New) / %.0f", lockedBits, newBits,
             g_state.targetBits);
  } else {
    snprintf(overlay, sizeof(overlay), "Entropy: %.0f / %.0f bits",
             g_state.collectedBits, g_state.targetBits);
  }

  ImVec2 textSize = ImGui::CalcTextSize(overlay);
  ImVec2 textPos = ImVec2(p_min.x + (width - textSize.x) * 0.5f,
                          p_min.y + (30 - textSize.y) * 0.5f);
  ImGui::GetWindowDrawList()->AddText(
      textPos, ImGui::GetColorU32(ImGuiCol_Text), overlay);

  ImGui::Dummy(ImVec2(width, 30)); // Advance cursor
  ImGui::PopStyleColor();

  // Quality indicator / Generation Mode
  ImGui::Text("Security Mode:");
  ImGui::SameLine();

  // Check if we have enough bits for 1:1 consolidation (True Randomness)
  if (g_state.isEntropyValid()) {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f),
                       "TRUE RANDOMNESS (Consolidation)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Input Entropy >= Target Output. \nWe will condense "
                        "raw data into perfect random bits. \nInformation "
                        "Theoretic Security possible (for OTP).");
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                       "PSEUDO-RANDOM (Expansion)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Input Entropy < Target Output. \nWe must use a CSPRNG to expand the "
          "key. \nComputationally Secure, but not 'True' Random for OTP.");
    }
  }

  // Status and buttons: anchor from the right so the two buttons never move.
  // Status text is right-aligned.
  const float barRightMargin = 24.0f;
  const float clearBtnW = 80.0f;
  const float stopStartBtnW = 160.0f;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  const float buttonStartX = ImGui::GetWindowWidth() - barRightMargin -
                             clearBtnW - spacing - stopStartBtnW;
  const float statusEndX = buttonStartX - spacing;

  ImGui::SameLine(ImGui::GetWindowWidth() - barRightMargin - clearBtnW -
                  spacing - stopStartBtnW -
                  120.0f); // move to same line, left of status area
  const char *statusStr =
      g_state.isCollecting ? "[Collecting...]" : "[Stopped]";
  ImVec2 statusSz = ImGui::CalcTextSize(statusStr);
  ImGui::SetCursorPosX(statusEndX - statusSz.x);
  if (g_state.isCollecting) {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "%s", statusStr);
  } else {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", statusStr);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", g_state.isCollecting ? "Collection in progress."
                                                 : "Collection stopped.");
  }

  ImGui::SameLine(buttonStartX);
  if (g_state.isCollecting) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.70f, 0.25f, 0.25f, 1.00f));
    if (ImGui::Button("Stop Collection", ImVec2(stopStartBtnW, 0))) {
      g_state.isCollecting = false;
    }
    ImGui::PopStyleColor(2);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.25f, 0.60f, 0.25f, 1.00f));
    if (ImGui::Button("Start Collection", ImVec2(stopStartBtnW, 0))) {
      g_state.isCollecting = true;
    }
    ImGui::PopStyleColor(2);
  }

  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.30f, 0.20f, 1.00f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.60f, 0.35f, 0.25f, 1.00f));
  if (ImGui::Button("Clear", ImVec2(clearBtnW, 0))) {
    // If >= 2048 bits, show confirmation dialog
    if (g_state.collectedBits >= 2048.0f) {
      ImGui::OpenPopup("Clear Data Confirmation");
    } else {
      // < 2048 bits: clear immediately
      ClearAllEntropyData();
    }
  }
  ImGui::PopStyleColor(2);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear all collected entropy data from all sources.\n"
                      "This will reset all counters to zero.\n"
                      "Collection will be stopped if active.");
  }

  // Confirmation dialog for >= 2048 bits
  if (ImGui::BeginPopupModal("Clear Data Confirmation", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(
        "Warning: You are about to clear %.0f bits of collected entropy.\n\n",
        g_state.collectedBits);
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

//=============================================================================
// SIMULATION
//=============================================================================

void SimulateEntropyCollection() {
  if (!g_state.isCollecting)
    return;

  // Note: This function is now only used for calculating totals from real
  // collectors. Unimplemented sources no longer generate fake data. Clock drift
  // is collected via ClockDriftCollector in main.cpp

  // Calculate total based ONLY on included (checked) sources
  float total = 0.0f;
  if (g_state.microphoneEnabled)
    total += g_state.entropyMic;
  if (g_state.keystrokeEnabled)
    total += g_state.entropyKeystroke;
  if (g_state.clockDriftEnabled)
    total += g_state.entropyClock;
  if (g_state.cpuJitterEnabled)
    total += g_state.entropyJitter;
  if (g_state.mouseMovementEnabled)
    total += g_state.entropyMouse;

  g_state.collectedBits = total;
}

//=============================================================================
// MODALS
//=============================================================================

void RenderNistProgressModal() {
    if (g_state.isExportingNist) {
        if (!ImGui::IsPopupOpen("Exporting NIST Data")) {
            ImGui::OpenPopup("Exporting NIST Data");
        }
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Exporting NIST Data", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::Text("Generating raw binary data for NIST SP 800-22...");
        ImGui::Separator();
        
        float progress = 0.0f;
        if (g_state.nistTotalBytes > 0) {
            progress = (float)g_state.nistBytesWritten / (float)g_state.nistTotalBytes;
        }
        
        char progressOverlay[64];
        snprintf(progressOverlay, sizeof(progressOverlay), "%.1f MB / %.1f MB", 
            (float)g_state.nistBytesWritten / (1024*1024), 
            (float)g_state.nistTotalBytes / (1024*1024));
            
        ImGui::ProgressBar(progress, ImVec2(300, 0), progressOverlay);
        
        if (!g_state.isExportingNist) {
            // Finished
            if (g_state.nistError.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Export Complete!");
                ImGui::Spacing();
                
                // Center the close button
                float windowWidth = ImGui::GetWindowSize().x;
                float buttonWidth = 120.0f;
                ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
                
                if (ImGui::Button("Close", ImVec2(buttonWidth, 0))) {
                    ImGui::CloseCurrentPopup();
                }
            } else {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Export Failed:");
                ImGui::TextWrapped("%s", g_state.nistError.c_str());
                ImGui::Spacing();
                
                float windowWidth = ImGui::GetWindowSize().x;
                float buttonWidth = 120.0f;
                ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);
                
                if (ImGui::Button("Close", ImVec2(buttonWidth, 0))) {
                    ImGui::CloseCurrentPopup();
                }
            }
        } else {
            // Still running - show spinner? Or just progress bar is fine.
            ImGui::Spacing();
            ImGui::TextDisabled("Please wait... (Generating 100MB)");
        }

        ImGui::EndPopup();
    }
}

void RenderAboutModal() {
    if (g_state.showAboutModal) {
        ImGui::OpenPopup("About TRNG");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("About TRNG", &g_state.showAboutModal, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("True Random Number Generator (TRNG)");
        ImGui::Text("Version 1.0.0");
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

void HandleExportOutput() {
    if (g_state.generatedOutput.empty()) {
        return;
    }

    char filename[MAX_PATH] = "output.txt";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0Binary Files (*.bin)\0*.bin\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "txt";

    if (GetSaveFileNameA(&ofn)) {
        std::ofstream out(filename, std::ios::binary);
        if (out.is_open()) {
            size_t size = g_state.generatedOutput.size();
            // Remove null terminator if present
            if (size > 0 && g_state.generatedOutput.back() == '\0') {
                size--;
            }
            out.write(g_state.generatedOutput.data(), size);
            out.close();
            Logger::Log(Logger::Level::INFO, "GUI", "Output exported to %s", filename);
        }
    }
}
