#include "gui.h"
#include "app_state.h"
#include "imgui.h"

//=============================================================================
// ENTROPY SOURCE TABS
//=============================================================================

void RenderMicrophoneTab() {
    ImGui::Text("Microphone Noise Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Captures ambient audio noise from the system microphone. "
                       "Environmental thermal noise provides high-quality entropy.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Microphone Collection", &g_state.microphoneEnabled);
    
    if (g_state.microphoneEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Collecting entropy...");
    } else if (g_state.microphoneEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderKeystrokeTab() {
    ImGui::Text("Keystroke Dynamics Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Measures timing intervals between keystrokes with microsecond precision. "
                       "Requires user typing to generate entropy.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Keystroke Collection", &g_state.keystrokeEnabled);
    
    if (g_state.keystrokeEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Type to generate entropy...");
    } else if (g_state.keystrokeEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderClockDriftTab() {
    ImGui::Text("CPU/System Clock Drift Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Measures drift between the high-resolution CPU cycle counter and the system clock. "
                       "Passive collection.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Clock Drift Collection", &g_state.clockDriftEnabled);
    
    if (g_state.clockDriftEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Collecting entropy...");
    } else if (g_state.clockDriftEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderCPUJitterTab() {
    ImGui::Text("CPU Execution Jitter Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Measures variations in CPU instruction execution times caused by "
                       "thermal noise, cache effects, and interrupts. Passive collection.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable CPU Jitter Collection", &g_state.cpuJitterEnabled);
    
    if (g_state.cpuJitterEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Collecting entropy...");
    } else if (g_state.cpuJitterEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}

void RenderMouseMovementTab() {
    ImGui::Text("Mouse Movement Collection");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Captures mouse position, velocity, and timing. "
                       "Requires user mouse movement to generate entropy.");
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Mouse Collection", &g_state.mouseMovementEnabled);
    
    if (g_state.mouseMovementEnabled && g_state.isCollecting) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Status: Active - Move mouse to generate entropy...");
    } else if (g_state.mouseMovementEnabled) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Status: Ready");
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Status: Disabled");
    }
    
    ImGui::Spacing();
    ImGui::Spacing();
    RenderStartStopButton();
}
