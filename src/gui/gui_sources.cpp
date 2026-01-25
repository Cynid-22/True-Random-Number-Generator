#include "gui.h"
#include "../core/app_state.h"
#include "../logging/logger.h"
#include "imgui.h"

//=============================================================================
// IMPLEMENTATION STATUS CONSTANTS
//=============================================================================

// Track which features are actually implemented
static const bool FEATURE_MICROPHONE_IMPLEMENTED = false;
static const bool FEATURE_KEYSTROKE_IMPLEMENTED = false;
static const bool FEATURE_MOUSE_IMPLEMENTED = false;
static const bool FEATURE_CLOCK_DRIFT_IMPLEMENTED = true;  // Has ClockDriftCollector
static const bool FEATURE_CPU_JITTER_IMPLEMENTED = false;

// Helper function to check if device is available (placeholder for future implementation)
// For now, since features aren't implemented, this always returns false
static bool IsDeviceAvailable(const char* deviceType) {
    // TODO: When features are implemented, check actual device availability
    // For microphone: check if audio device is connected and accessible
    // For mouse: check if mouse input is being received
    // For keystroke: keyboard is always available
    return false; // Placeholder
}

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
    if (ImGui::Checkbox("Include Microphone in Final Calculation", &g_state.microphoneEnabled)) {
        Logger::Log(Logger::Level::INFO, "GUI", "Microphone source toggled: %s", g_state.microphoneEnabled ? "ON" : "OFF");
    }
    
    // Status display with implementation check
    ImGui::SameLine();
    if (!FEATURE_MICROPHONE_IMPLEMENTED) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo data is being collected from this source.");
        }
    } else if (!g_state.microphoneEnabled) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This source is disabled by the user and will not contribute to entropy collection.");
        }
    } else if (g_state.isCollecting) {
        // Check if device is available
        bool deviceAvailable = IsDeviceAvailable("microphone");
        if (!deviceAvailable && g_state.entropyMic > 0.0f) {
            // Device was collecting but is now unavailable
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("This input device/mode is currently unavailable.\n\n"
                    "Possible reasons:\n"
                    "- Device unplugged/disconnected\n"
                    "- Device not recognized by system\n"
                    "- Driver issues or missing drivers\n"
                    "- Permission denied (e.g., microphone access blocked)\n"
                    "- Hardware failure\n"
                    "- System-level problems preventing access\n\n"
                    "Past collected entropy from this source is still included in the total calculation.\n"
                    "No new data is being collected from this source while unavailable.\n"
                    "Resolve the issue (reconnect device, check permissions, fix drivers, etc.) to resume collection.");
            }
        } else if (deviceAvailable) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Microphone is actively collecting entropy.\n"
                    "Collected: %.1f bits\n"
                    "Source is working correctly.", g_state.entropyMic);
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("This input device/mode is currently unavailable.\n\n"
                    "Possible reasons:\n"
                    "- Device unplugged/disconnected\n"
                    "- Device not recognized by system\n"
                    "- Driver issues or missing drivers\n"
                    "- Permission denied (e.g., microphone access blocked)\n"
                    "- Hardware failure\n"
                    "- System-level problems preventing access\n\n"
                    "Past collected entropy from this source is still included in the total calculation.\n"
                    "No new data is being collected from this source while unavailable.\n"
                    "Resolve the issue (reconnect device, check permissions, fix drivers, etc.) to resume collection.");
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Microphone is enabled and ready to collect data when collection starts.");
        }
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
    if (ImGui::Checkbox("Include Keystroke in Final Calculation", &g_state.keystrokeEnabled)) {
        Logger::Log(Logger::Level::INFO, "GUI", "Keystroke source toggled: %s", g_state.keystrokeEnabled ? "ON" : "OFF");
    }
    
    // Status display with implementation check
    ImGui::SameLine();
    if (!FEATURE_KEYSTROKE_IMPLEMENTED) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo data is being collected from this source.");
        }
    } else if (!g_state.keystrokeEnabled) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This source is disabled by the user and will not contribute to entropy collection.");
        }
    } else if (g_state.isCollecting) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Keystroke dynamics are actively being collected.\n"
                "Collected: %.1f bits\n"
                "Source is working correctly.", g_state.entropyKeystroke);
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Keystroke capture is enabled and ready to collect data when collection starts.");
        }
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
    if (ImGui::Checkbox("Include Mouse in Final Calculation", &g_state.mouseMovementEnabled)) {
        Logger::Log(Logger::Level::INFO, "GUI", "Mouse source toggled: %s", g_state.mouseMovementEnabled ? "ON" : "OFF");
    }
    
    // Status display with implementation check
    ImGui::SameLine();
    if (!FEATURE_MOUSE_IMPLEMENTED) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo data is being collected from this source.");
        }
    } else if (!g_state.mouseMovementEnabled) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This source is disabled by the user and will not contribute to entropy collection.");
        }
    } else if (g_state.isCollecting) {
        // Check if mouse input is being received
        bool deviceAvailable = IsDeviceAvailable("mouse");
        if (!deviceAvailable && g_state.entropyMouse > 0.0f) {
            // Mouse was collecting but is now unavailable
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("This input device/mode is currently unavailable.\n\n"
                    "Possible reasons:\n"
                    "- Device unplugged/disconnected\n"
                    "- Device not recognized by system\n"
                    "- Driver issues or missing drivers\n"
                    "- Permission denied (e.g., mouse access blocked)\n"
                    "- Hardware failure\n"
                    "- System-level problems preventing access\n"
                    "- No mouse input detected\n\n"
                    "Past collected entropy from this source is still included in the total calculation.\n"
                    "No new data is being collected from this source while unavailable.\n"
                    "Resolve the issue (reconnect device, check permissions, fix drivers, move mouse, etc.) to resume collection.");
            }
        } else if (deviceAvailable) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Mouse movement is actively being tracked.\n"
                    "Collected: %.1f bits\n"
                    "Source is working correctly.", g_state.entropyMouse);
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("This input device/mode is currently unavailable.\n\n"
                    "Possible reasons:\n"
                    "- Device unplugged/disconnected\n"
                    "- Device not recognized by system\n"
                    "- Driver issues or missing drivers\n"
                    "- Permission denied (e.g., mouse access blocked)\n"
                    "- Hardware failure\n"
                    "- System-level problems preventing access\n"
                    "- No mouse input detected\n\n"
                    "Past collected entropy from this source is still included in the total calculation.\n"
                    "No new data is being collected from this source while unavailable.\n"
                    "Resolve the issue (reconnect device, check permissions, fix drivers, move mouse, etc.) to resume collection.");
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Mouse movement tracking is enabled and ready to collect data when collection starts.");
        }
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
    if (ImGui::Checkbox("Include Clock Drift in Final Calculation", &g_state.clockDriftEnabled)) {
         Logger::Log(Logger::Level::INFO, "GUI", "Clock Drift source toggled: %s", g_state.clockDriftEnabled ? "ON" : "OFF");
    }
    
    // Status display (Clock Drift is implemented)
    if (!g_state.clockDriftEnabled) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This source is disabled by the user and will not contribute to entropy collection.");
        }
    } else if (g_state.isCollecting) {
        ImGui::SameLine();
        if (g_state.clockDriftCollector.IsRunning()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Clock Drift collector is actively running.\n\n"
                    "Samples collected: %llu\n"
                    "Collection rate: %.0f samples/sec\n"
                    "Collected entropy: %.1f bits\n"
                    "Estimated entropy per sample: ~2 bits\n"
                    "Source is working correctly.",
                    g_state.clockDriftCollector.GetSampleCount(),
                    g_state.clockDriftCollector.GetEntropyRate(),
                    g_state.entropyClock);
            }
            ImGui::Text("    Samples: %llu | Rate: %.0f samples/sec", 
                g_state.clockDriftCollector.GetSampleCount(),
                g_state.clockDriftCollector.GetEntropyRate());
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Starting...]");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Clock Drift collector is starting up.\n"
                    "The background thread is initializing.\n"
                    "Collection will begin shortly.");
            }
        }
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Clock Drift collection is enabled and ready.\n"
                "The collector will start automatically when collection begins.\n"
                "This source uses CPU cycle counter measurements to extract hardware entropy.");
        }
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
    if (ImGui::Checkbox("Include CPU Jitter in Final Calculation", &g_state.cpuJitterEnabled)) {
         Logger::Log(Logger::Level::INFO, "GUI", "CPU Jitter source toggled: %s", g_state.cpuJitterEnabled ? "ON" : "OFF");
    }
    
    // Status display with implementation check
    ImGui::SameLine();
    if (!FEATURE_CPU_JITTER_IMPLEMENTED) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo data is being collected from this source.");
        }
    } else if (!g_state.cpuJitterEnabled) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("This source is disabled by the user and will not contribute to entropy collection.");
        }
    } else if (g_state.isCollecting) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("CPU Jitter race condition is actively running.\n"
                "Collected: %.1f bits\n"
                "Thread race is generating entropy from OS scheduling jitter.\n"
                "Source is working correctly.", g_state.entropyJitter);
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("CPU Jitter collection is enabled and ready.\n"
                "The race condition threads will start automatically when collection begins.\n"
                "This source exploits OS scheduling unpredictability for entropy.");
        }
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
        
        // Microphone
        if (!FEATURE_MICROPHONE_IMPLEMENTED) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Microphone: NOT IMPLEMENTED");
        } else if (!g_state.microphoneEnabled) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Microphone: DEACTIVATED");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Microphone: ACTIVE");
        }
        
        // Keystroke
        if (!FEATURE_KEYSTROKE_IMPLEMENTED) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Keystroke: NOT IMPLEMENTED");
        } else if (!g_state.keystrokeEnabled) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Keystroke: DEACTIVATED");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Keystroke: ACTIVE");
        }
        
        // Mouse
        if (!FEATURE_MOUSE_IMPLEMENTED) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Mouse: NOT IMPLEMENTED");
        } else if (!g_state.mouseMovementEnabled) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Mouse: DEACTIVATED");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Mouse: ACTIVE");
        }
        
        // Clock Drift
        if (!g_state.clockDriftEnabled) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  Clock Drift: DEACTIVATED");
        } else if (g_state.clockDriftCollector.IsRunning()) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  Clock Drift: ACTIVE");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "  Clock Drift: STARTING...");
        }
        
        // CPU Jitter
        if (!FEATURE_CPU_JITTER_IMPLEMENTED) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  CPU Jitter: NOT IMPLEMENTED");
        } else if (!g_state.cpuJitterEnabled) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "  CPU Jitter: DEACTIVATED");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "  CPU Jitter: ACTIVE");
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
