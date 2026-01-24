#include "gui.h"
#include "app_state.h"
#include "imgui.h"

//=============================================================================
// ENTROPY SOURCE TABS
//=============================================================================

void RenderUserInputTab() {
    ImGui::Text("User Input Sources");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("Configure which user-generated entropy sources to include in the final calculation.");
    ImGui::Spacing();
    ImGui::Spacing();
    
    // 1. Microphone Section
    ImGui::Text("Microphone Noise (Thermal Entropy)");
    ImGui::Indent();
    if (ImGui::CollapsingHeader("How it works##mic")) {
        ImGui::TextWrapped("Captures the Least Significant Bit (LSB) of audio samples. "
                           "This bit is determined by thermal noise (electrons bouncing due to heat), "
                           "not actual sound. High sample rate (44.1kHz) provides ~44,000 random bits/sec.");
    }
    ImGui::Checkbox("Include Microphone in Final Calculation", &g_state.microphoneEnabled);
    if (g_state.microphoneEnabled && g_state.isCollecting) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
    } else if (g_state.microphoneEnabled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    }
    ImGui::Unindent();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // 2. Keystroke Section
    ImGui::Text("Keystroke Dynamics (User Entropy)");
    ImGui::Indent();
    if (ImGui::CollapsingHeader("How it works##key")) {
        ImGui::TextWrapped("Captures your unique typing rhythm: Flight Time (gap between keys) "
                           "and Dwell Time (how long each key is held). These timings are measured "
                           "in nanoseconds and are unique to each person.");
    }
    ImGui::Checkbox("Include Keystroke in Final Calculation", &g_state.keystrokeEnabled);
    if (g_state.keystrokeEnabled && g_state.isCollecting) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
    } else if (g_state.keystrokeEnabled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    }
    ImGui::Unindent();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // 3. Mouse Movement Section
    ImGui::Text("Mouse Movement (User Entropy)");
    ImGui::Indent();
    if (ImGui::CollapsingHeader("How it works##mouse")) {
        ImGui::TextWrapped("Records X/Y coordinates and precise timestamps (nanoseconds) of mouse movements. "
                           "Your physical motor noise creates unpredictable patterns. "
                           "Small movements (<3 pixels) are filtered to avoid sensor drift.");
    }
    ImGui::Checkbox("Include Mouse in Final Calculation", &g_state.mouseMovementEnabled);
    if (g_state.mouseMovementEnabled && g_state.isCollecting) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
    } else if (g_state.mouseMovementEnabled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    }
    ImGui::Unindent();
}

void RenderSystemInputTab() {
    ImGui::Text("System Input Sources (Hardware/OS Entropy)");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextWrapped("These sources exploit hardware imperfections and OS scheduling chaos. "
                       "They run in parallel to intentionally create CPU 'traffic jams' for maximum entropy.");
    ImGui::Spacing();
    
    // Detailed Explanation Section
    if (ImGui::CollapsingHeader("Why Parallel Execution?")) {
        ImGui::TextWrapped("Sequential execution is predictable. Parallel execution forces the CPU to "
                           "context-switch, causing cache contention and thermal variations.");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // 1. Clock Drift Section
    ImGui::Text("Clock Drift (Hardware Entropy)");
    ImGui::Indent();
    if (ImGui::CollapsingHeader("How it works##clock")) {
        ImGui::TextWrapped("Measures the delta in CPU cycle counts during a fixed System Time window. "
                           "A 3GHz CPU should count ~3,000,000 cycles per ms, but due to heat/voltage, "
                           "the actual count varies (e.g., 3,000,402 or 2,999,881). Those fluctuating digits are entropy.");
    }
    ImGui::Checkbox("Include Clock Drift in Final Calculation", &g_state.clockDriftEnabled);
    if (g_state.clockDriftEnabled && g_state.isCollecting) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active - Measuring drift]");
    } else if (g_state.clockDriftEnabled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    }
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. CPU Jitter Section
    ImGui::Text("CPU Jitter (System Entropy)");
    ImGui::Indent();
    if (ImGui::CollapsingHeader("How it works##jitter")) {
        ImGui::TextWrapped("Creates a 'race condition': Thread A counts up infinitely, Thread B periodically "
                           "freezes it and reads the count. The exact count depends on OS scheduling, "
                           "background tasks (WiFi, updates), making it unpredictable.");
    }
    ImGui::Checkbox("Include CPU Jitter in Final Calculation", &g_state.cpuJitterEnabled);
    if (g_state.cpuJitterEnabled && g_state.isCollecting) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active - Race running]");
    } else if (g_state.cpuJitterEnabled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    }
    ImGui::Unindent();
}

//=============================================================================
// COLLECTION WINDOW (Popup)
//=============================================================================

void RenderCollectionWindow() {
    if (!g_state.isCollecting) return;
    
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Collection Window", &g_state.isCollecting, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Entropy Collection in Progress...");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Placeholder content
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[PLACEHOLDER]");
        ImGui::Spacing();
        
        ImGui::TextWrapped("This window will display:");
        ImGui::BulletText("Mouse path visualization (trail of movement)");
        ImGui::BulletText("Single-line textbox showing typed characters");
        ImGui::BulletText("Status indicators for Microphone, Clock Drift, CPU Jitter");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Show current source status
        ImGui::Text("Source Status:");
        
        if (g_state.microphoneEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Microphone: ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Microphone: DEACTIVATED");
        }
        
        if (g_state.keystrokeEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Keystroke: ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Keystroke: DEACTIVATED");
        }
        
        if (g_state.mouseMovementEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Mouse: ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Mouse: DEACTIVATED");
        }
        
        if (g_state.clockDriftEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Clock Drift: ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Clock Drift: DEACTIVATED");
        }
        
        if (g_state.cpuJitterEnabled) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  CPU Jitter: ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  CPU Jitter: DEACTIVATED");
        }
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Stop button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.25f, 0.25f, 1.00f));
        if (ImGui::Button("Stop Collection", ImVec2(180, 0))) {
            g_state.isCollecting = false;
        }
        ImGui::PopStyleColor(2);
    }
    ImGui::End();
}
