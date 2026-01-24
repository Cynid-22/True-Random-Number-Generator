// TRNG - GUI Implementation
// Contains core GUI rendering functions: Style, Menu, Layout, Common widgets

#include "gui.h"
#include "app_state.h"
#include "logic.h"
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
                g_state = AppState();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About TRNG")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

//=============================================================================
// HELPER: Render Start/Stop Button (used in each tab)
//=============================================================================

void RenderStartStopButton() {
    if (g_state.isCollecting) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.00f));
        if (ImGui::Button("Stop Collection", ImVec2(150, 0))) {
            g_state.isCollecting = false;
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.60f, 0.25f, 1.00f));
        if (ImGui::Button("Start Collection", ImVec2(150, 0))) {
            g_state.isCollecting = true;
        }
        ImGui::PopStyleColor(2);
    }
}

//=============================================================================
// ENTROPY POOL BAR
//=============================================================================

void RenderEntropyPoolBar() {
    // Entropy progress bar - always visible at top
    float progress = g_state.collectedBits / g_state.targetBits;
    if (progress > 1.0f) progress = 1.0f;
    
    char overlay[64];
    snprintf(overlay, sizeof(overlay), "Entropy: %.0f / %.0f bits", g_state.collectedBits, g_state.targetBits);
    ImGui::ProgressBar(progress, ImVec2(-1, 30), overlay);  // Taller bar for larger font
    
    // Quality indicator
    ImGui::Text("Quality:");
    ImGui::SameLine();
    if (progress < 0.25f) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Insufficient");
    } else if (progress < 0.5f) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Low");
    } else if (progress < 1.0f) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Good");
    } else {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Excellent");
    }
    
    // Show collection status indicator
    ImGui::SameLine(ImGui::GetWindowWidth() - 120);
    if (g_state.isCollecting) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Collecting...]");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Stopped]");
    }
}

//=============================================================================
// SIMULATION
//=============================================================================

void SimulateEntropyCollection() {
    if (!g_state.isCollecting) return;
    
    // Add small random amount based on enabled sources
    float entropyToAdd = 0.0f;
    
    if (g_state.microphoneEnabled) entropyToAdd += 0.5f;
    if (g_state.keystrokeEnabled) entropyToAdd += 0.2f; // Simulate typing occasionally
    if (g_state.clockDriftEnabled) entropyToAdd += 0.1f;
    if (g_state.cpuJitterEnabled) entropyToAdd += 0.3f;
    if (g_state.mouseMovementEnabled) entropyToAdd += 0.4f; // Simulate movement
    
    // Add to pool
    g_state.collectedBits += entropyToAdd;
    
    // Cap at reasonable max for this demo
    if (g_state.collectedBits > 2048.0f) g_state.collectedBits = 2048.0f;
    
    // Consume entropy if we generated something (fake consumption for visual feedback)
    if (g_state.entropyConsumed > 0) {
        g_state.collectedBits -= g_state.entropyConsumed;
        g_state.entropyConsumed = 0;
        if (g_state.collectedBits < 0) g_state.collectedBits = 0;
    }
}
