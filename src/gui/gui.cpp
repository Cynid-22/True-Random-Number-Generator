// TRNG - GUI Implementation
// Contains core GUI rendering functions: Style, Menu, Layout, Common widgets

#include "gui.h"
#include "../core/app_state.h"
#include "../logic/logic.h"
#include "../logging/logger.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <cstdio>

//=============================================================================
// STYLE SETUP
//=============================================================================

void SetupNativeStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
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
    ImVec4* colors = style.Colors;
    
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
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Export Output...", "Ctrl+S")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                PostQuitMessage(0);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Auto-start collection", nullptr, &g_state.isCollecting);
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
        if (ImGui::BeginMenu("Debug")) {
            // Checkbox for logging
            bool keepLogs = g_state.keepLogs;
            if (ImGui::MenuItem("Keep Logs", nullptr, &keepLogs)) {
                g_state.keepLogs = keepLogs;
                Logger::SetEnabled(g_state.keepLogs);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About TRNG")) {}
            ImGui::EndMenu();
        }
        
        // Global Warning if Logging is ON
        if (g_state.keepLogs) {
            // Move further left to prevent clipping
            ImGui::SameLine(ImGui::GetWindowWidth() - 250);
            
            // Add a background box for visibility
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.0f, 0.0f, 1.0f)); // Dark red bg
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));   // Bright red text
            ImGui::SmallButton("!! LOGGING ENABLED !!"); // Use Button for box effect
            ImGui::PopStyleColor(2);
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Debug logs are being written to disk.\nThis WILL compromise security, degrade performance, and fill storage.\nOnly turn this on if you are debugging an issue.");
            }
        }
        
        ImGui::EndMenuBar();
    }
}

//=============================================================================
// ENTROPY POOL BAR (with Start/Stop Button)
//=============================================================================

//=============================================================================
// ENTROPY POOL BAR
//=============================================================================

void RenderEntropyPoolBar() {
    // Recalculate total bits based on currently included sources
    // This allows the bar to update immediately when checkboxes are toggled
    float total = 0.0f;
    if (g_state.microphoneEnabled) total += g_state.entropyMic;
    if (g_state.keystrokeEnabled) total += g_state.entropyKeystroke;
    if (g_state.clockDriftEnabled) total += g_state.entropyClock;
    if (g_state.cpuJitterEnabled) total += g_state.entropyJitter;
    if (g_state.mouseMovementEnabled) total += g_state.entropyMouse;
    g_state.collectedBits = total;

    // Entropy progress bar - always visible at top
    float progress = g_state.collectedBits / g_state.targetBits;
    if (progress > 1.0f) progress = 1.0f;
    
    char overlay[64];
    snprintf(overlay, sizeof(overlay), "Entropy: %.0f / %.0f bits", g_state.collectedBits, g_state.targetBits);
    ImGui::ProgressBar(progress, ImVec2(-1, 30), overlay);  // Taller bar for larger font
    
    // Quality indicator / Generation Mode
    ImGui::Text("Security Mode:");
    ImGui::SameLine();
    
    // Check if we have enough bits for 1:1 consolidation (True Randomness)
    if (g_state.isEntropyValid()) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "TRUE RANDOMNESS (Consolidation)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Input Entropy >= Target Output. \nWe will condense raw data into perfect random bits. \nInformation Theoretic Security possible (for OTP).");
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "PSEUDO-RANDOM (Expansion)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Input Entropy < Target Output. \nWe must use a CSPRNG to expand the key. \nComputationally Secure, but not 'True' Random for OTP.");
        }
    }
    
    // Show collection status indicator and Start/Stop button
    ImGui::SameLine(ImGui::GetWindowWidth() - 350);
    if (g_state.isCollecting) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Collecting...]");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.00f));
        if (ImGui::Button("Stop Collection", ImVec2(160, 0))) {
            g_state.isCollecting = false;
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Stopped]");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.25f, 1.00f));
        if (ImGui::Button("Start Collection", ImVec2(160, 0))) {
            g_state.isCollecting = true;
        }
        ImGui::PopStyleColor(2);
    }
}

//=============================================================================
// SIMULATION
//=============================================================================

void SimulateEntropyCollection() {
    if (!g_state.isCollecting) return;
    
    // Note: This function is now only used for calculating totals from real collectors.
    // Unimplemented sources no longer generate fake data.
    // Clock drift is collected via ClockDriftCollector in main.cpp
    
    // Calculate total based ONLY on included (checked) sources
    float total = 0.0f;
    if (g_state.microphoneEnabled) total += g_state.entropyMic;
    if (g_state.keystrokeEnabled) total += g_state.entropyKeystroke;
    if (g_state.clockDriftEnabled) total += g_state.entropyClock;
    if (g_state.cpuJitterEnabled) total += g_state.entropyJitter;
    if (g_state.mouseMovementEnabled) total += g_state.entropyMouse;
    
    g_state.collectedBits = total;
}
