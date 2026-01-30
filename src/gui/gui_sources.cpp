#include "../core/app_state.h"
#include "../logging/logger.h"
#include "gui.h"
#include "imgui.h"


//=============================================================================
// IMPLEMENTATION STATUS CONSTANTS
//=============================================================================

// Track which features are actually implemented
static const bool FEATURE_MICROPHONE_IMPLEMENTED = false;
static const bool FEATURE_KEYSTROKE_IMPLEMENTED = false;
static const bool FEATURE_MOUSE_IMPLEMENTED = false;
static const bool FEATURE_CLOCK_DRIFT_IMPLEMENTED =
    true; // Has ClockDriftCollector
static const bool FEATURE_CPU_JITTER_IMPLEMENTED = true;

// Helper function to check if device is available (placeholder for future
// implementation) For now, since features aren't implemented, this always
// returns false
static bool IsDeviceAvailable(const char *deviceType) {
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

  ImGui::TextWrapped("Configure which user-generated entropy sources to "
                     "include in the final calculation.");
  ImGui::Spacing();
  ImGui::Spacing();

  // 1. Microphone Section
  ImGui::Text("Microphone Noise (Thermal Entropy)");
  ImGui::Indent();
  if (ImGui::CollapsingHeader("How it works##mic")) {
    ImGui::TextWrapped(
        "Captures the Least Significant Bit (LSB) of audio samples. "
        "This bit is determined by thermal noise (electrons bouncing due to "
        "heat), "
        "not actual sound. High sample rate (44.1kHz) provides ~44,000 random "
        "bits/sec.");
  }
  if (ImGui::Checkbox("Include Microphone in Final Calculation",
                      &g_state.microphoneEnabled)) {
    Logger::Log(Logger::Level::INFO, "GUI", "Microphone source toggled: %s",
                g_state.microphoneEnabled ? "ON" : "OFF");
  }

  // Status display with implementation check
  ImGui::SameLine();
  if (!FEATURE_MICROPHONE_IMPLEMENTED) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo "
                        "data is being collected from this source.");
    }
  } else if (!g_state.microphoneEnabled) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This source is disabled by the user and will not "
                        "contribute to entropy collection.");
    }
  } else if (g_state.isCollecting) {
    // Check if device is available
    bool deviceAvailable = IsDeviceAvailable("microphone");
    if (!deviceAvailable && g_state.entropyMic > 0.0f) {
      // Device was collecting but is now unavailable
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "This input device/mode is currently unavailable.\n\n"
            "Possible reasons:\n"
            "- Device unplugged/disconnected\n"
            "- Device not recognized by system\n"
            "- Driver issues or missing drivers\n"
            "- Permission denied (e.g., microphone access blocked)\n"
            "- Hardware failure\n"
            "- System-level problems preventing access\n\n"
            "Past collected entropy from this source is still included in the "
            "total calculation.\n"
            "No new data is being collected from this source while "
            "unavailable.\n"
            "Resolve the issue (reconnect device, check permissions, fix "
            "drivers, etc.) to resume collection.");
      }
    } else if (deviceAvailable) {
      ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Microphone is actively collecting entropy.\n"
                          "Collected: %.1f bits\n"
                          "Source is working correctly.",
                          g_state.entropyMic);
      }
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "This input device/mode is currently unavailable.\n\n"
            "Possible reasons:\n"
            "- Device unplugged/disconnected\n"
            "- Device not recognized by system\n"
            "- Driver issues or missing drivers\n"
            "- Permission denied (e.g., microphone access blocked)\n"
            "- Hardware failure\n"
            "- System-level problems preventing access\n\n"
            "Past collected entropy from this source is still included in the "
            "total calculation.\n"
            "No new data is being collected from this source while "
            "unavailable.\n"
            "Resolve the issue (reconnect device, check permissions, fix "
            "drivers, etc.) to resume collection.");
      }
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Microphone is enabled and ready to collect data when "
                        "collection starts.");
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
    ImGui::TextWrapped(
        "Captures your unique typing rhythm: Flight Time (gap between keys) "
        "and Dwell Time (how long each key is held). These timings are "
        "measured "
        "in nanoseconds and are unique to each person.");
  }
  if (ImGui::Checkbox("Include Keystroke in Final Calculation",
                      &g_state.keystrokeEnabled)) {
    Logger::Log(Logger::Level::INFO, "GUI", "Keystroke source toggled: %s",
                g_state.keystrokeEnabled ? "ON" : "OFF");
  }

  // Status display with implementation check
  ImGui::SameLine();
  if (!FEATURE_KEYSTROKE_IMPLEMENTED) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo "
                        "data is being collected from this source.");
    }
  } else if (!g_state.keystrokeEnabled) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This source is disabled by the user and will not "
                        "contribute to entropy collection.");
    }
  } else if (g_state.isCollecting) {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Keystroke dynamics are actively being collected.\n"
                        "Collected: %.1f bits\n"
                        "Source is working correctly.",
                        g_state.entropyKeystroke);
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Keystroke capture is enabled and ready to collect "
                        "data when collection starts.");
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
    ImGui::TextWrapped(
        "Records X/Y coordinates and precise timestamps (nanoseconds) of mouse "
        "movements. "
        "Your physical motor noise creates unpredictable patterns. "
        "Small movements (<3 pixels) are filtered to avoid sensor drift.");
  }
  if (ImGui::Checkbox("Include Mouse in Final Calculation",
                      &g_state.mouseMovementEnabled)) {
    Logger::Log(Logger::Level::INFO, "GUI", "Mouse source toggled: %s",
                g_state.mouseMovementEnabled ? "ON" : "OFF");
  }

  // Status display with implementation check
  ImGui::SameLine();
  if (!FEATURE_MOUSE_IMPLEMENTED) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo "
                        "data is being collected from this source.");
    }
  } else if (!g_state.mouseMovementEnabled) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This source is disabled by the user and will not "
                        "contribute to entropy collection.");
    }
  } else if (g_state.isCollecting) {
    // Check if mouse input is being received
    bool deviceAvailable = IsDeviceAvailable("mouse");
    if (!deviceAvailable && g_state.entropyMouse > 0.0f) {
      // Mouse was collecting but is now unavailable
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "This input device/mode is currently unavailable.\n\n"
            "Possible reasons:\n"
            "- Device unplugged/disconnected\n"
            "- Device not recognized by system\n"
            "- Driver issues or missing drivers\n"
            "- Permission denied (e.g., mouse access blocked)\n"
            "- Hardware failure\n"
            "- System-level problems preventing access\n"
            "- No mouse input detected\n\n"
            "Past collected entropy from this source is still included in the "
            "total calculation.\n"
            "No new data is being collected from this source while "
            "unavailable.\n"
            "Resolve the issue (reconnect device, check permissions, fix "
            "drivers, move mouse, etc.) to resume collection.");
      }
    } else if (deviceAvailable) {
      ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Mouse movement is actively being tracked.\n"
                          "Collected: %.1f bits\n"
                          "Source is working correctly.",
                          g_state.entropyMouse);
      }
    } else {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[UNAVAILABLE]");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "This input device/mode is currently unavailable.\n\n"
            "Possible reasons:\n"
            "- Device unplugged/disconnected\n"
            "- Device not recognized by system\n"
            "- Driver issues or missing drivers\n"
            "- Permission denied (e.g., mouse access blocked)\n"
            "- Hardware failure\n"
            "- System-level problems preventing access\n"
            "- No mouse input detected\n\n"
            "Past collected entropy from this source is still included in the "
            "total calculation.\n"
            "No new data is being collected from this source while "
            "unavailable.\n"
            "Resolve the issue (reconnect device, check permissions, fix "
            "drivers, move mouse, etc.) to resume collection.");
      }
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Mouse movement tracking is enabled and ready to "
                        "collect data when collection starts.");
    }
  }
  ImGui::Unindent();
}

void RenderSystemInputTab() {
  ImGui::Text("System Input Sources (Hardware/OS Entropy)");
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::TextWrapped(
      "These sources exploit hardware imperfections and OS scheduling chaos. "
      "They run in parallel to intentionally create CPU 'traffic jams' for "
      "maximum entropy.");
  ImGui::Spacing();

  // Detailed Explanation Section
  if (ImGui::CollapsingHeader("Why Parallel Execution?")) {
    ImGui::TextWrapped(
        "Sequential execution is predictable. Parallel execution forces the "
        "CPU to "
        "context-switch, causing cache contention and thermal variations.");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // 1. Clock Drift Section
  ImGui::Text("Clock Drift (Hardware Entropy)");
  ImGui::Indent();
  if (ImGui::CollapsingHeader("How it works##clock")) {
    ImGui::TextWrapped("Measures the delta in CPU cycle counts during a fixed "
                       "System Time window. "
                       "A 3GHz CPU should count ~3,000,000 cycles per ms, but "
                       "due to heat/voltage, "
                       "the actual count varies (e.g., 3,000,402 or "
                       "2,999,881). Those fluctuating digits are entropy.");
  }
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

  // Calculate total based on lock-in logic:
  // 1. Locked entropy (everything <= lockedTimestamp) - Always included
  // 2. New entropy (everything > lockedTimestamp) - Only if source is enabled
  g_state.collectedBits = g_state.entropyPool.GetTotalBits(
      g_state.lockedDataTimestamp, enabledSources);
  if (ImGui::Checkbox("Include Clock Drift in Final Calculation",
                      &g_state.clockDriftEnabled)) {
    Logger::Log(Logger::Level::INFO, "GUI", "Clock Drift source toggled: %s",
                g_state.clockDriftEnabled ? "ON" : "OFF");
  }

  // Status display (Clock Drift is implemented)
  if (!g_state.clockDriftEnabled) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This source is disabled by the user and will not "
                        "contribute to entropy collection.");
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
      ImGui::SetTooltip(
          "Clock Drift collection is enabled and ready.\n"
          "The collector will start automatically when collection begins.\n"
          "This source uses CPU cycle counter measurements to extract hardware "
          "entropy.");
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
    ImGui::TextWrapped(
        "Creates a 'race condition': Thread A counts up infinitely, Thread B "
        "periodically "
        "freezes it and reads the count. The exact count depends on OS "
        "scheduling, "
        "background tasks (WiFi, updates), making it unpredictable.");
  }
  if (ImGui::Checkbox("Include CPU Jitter in Final Calculation",
                      &g_state.cpuJitterEnabled)) {
    Logger::Log(Logger::Level::INFO, "GUI", "CPU Jitter source toggled: %s",
                g_state.cpuJitterEnabled ? "ON" : "OFF");
  }

  // Status display with implementation check
  ImGui::SameLine();
  if (!FEATURE_CPU_JITTER_IMPLEMENTED) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[NOT IMPLEMENTED]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This feature is planned but not yet implemented.\nNo "
                        "data is being collected from this source.");
    }
  } else if (!g_state.cpuJitterEnabled) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Excluded]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This source is disabled by the user and will not "
                        "contribute to entropy collection.");
    }
  } else if (g_state.isCollecting) {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[Active]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "CPU Jitter race condition is actively running.\n"
          "Collected: %.1f bits\n"
          "Thread race is generating entropy from OS scheduling jitter.\n"
          "Source is working correctly.",
          g_state.entropyJitter);
    }
  } else {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[Ready]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "CPU Jitter collection is enabled and ready.\n"
          "The race condition threads will start automatically when collection "
          "begins.\n"
          "This source exploits OS scheduling unpredictability for entropy.");
    }
  }
  ImGui::Unindent();
}

//=============================================================================
// COLLECTION WINDOW (Popup)
//=============================================================================

void RenderCollectionWindow() {
  if (!g_state.isCollecting)
    return;

  // VISUALIZATION CAPTURE: Keystrokes (Global while window is active)
  ImGuiIO& io = ImGui::GetIO();
  for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
      ImWchar c = io.InputQueueCharacters[i];
      if (c >= 32 && c < 127) { // ASCII printable
          g_state.keystrokePreview += (char)c;
      }
  }
  if (g_state.keystrokePreview.length() > 500) {
       g_state.keystrokePreview = g_state.keystrokePreview.substr(g_state.keystrokePreview.length() - 500);
  }

  // Center window on screen when it appears
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 work_size = viewport->WorkSize;
  
  // Size: 80% width, 85% height (increased from 80%)
  ImVec2 window_size = ImVec2(work_size.x * 0.8f, work_size.y * 0.82f);
  
  if (window_size.x < 1000.0f) window_size.x = 1000.0f;
  if (window_size.y < 700.0f) window_size.y = 700.0f;
  
  ImGui::SetNextWindowSize(window_size, ImGuiCond_Appearing);
  
  // Position: Center X, Center Y + 5% offset (Lower)
  ImVec2 centerPos = viewport->GetCenter();
  centerPos.y += work_size.y * 0.05f; 
  
  ImGui::SetNextWindowPos(centerPos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::Begin("Collection Window", &g_state.isCollecting,
                   ImGuiWindowFlags_NoCollapse)) {
    // 1. Status Section (Top)
    ImGui::Text("Status:");
    
    // Use columns to save vertical space
    ImGui::Columns(3, "status_columns", false); 
    
    // Helper lambda for status line
    auto StatusLine = [](const char* label, bool implemented, bool enabled, bool active, float entropy) {
        ImGui::Text("%s:", label);
        ImGui::SameLine();
        if (!implemented) ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f), "N/A");
        else if (!enabled) ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f), "OFF");
        else if (active) ImGui::TextColored(ImVec4(0.3f,1.0f,0.5f,1.0f), "ACTIVE");
        else ImGui::TextColored(ImVec4(1.0f,0.8f,0.3f,1.0f), "WAIT");
    };

    StatusLine("Mic", FEATURE_MICROPHONE_IMPLEMENTED, g_state.microphoneEnabled, true, g_state.entropyMic);
    ImGui::NextColumn();
    StatusLine("Keys", FEATURE_KEYSTROKE_IMPLEMENTED, g_state.keystrokeEnabled, true, g_state.entropyKeystroke);
    ImGui::NextColumn();
    StatusLine("Mouse", FEATURE_MOUSE_IMPLEMENTED, g_state.mouseMovementEnabled, true, g_state.entropyMouse);
    ImGui::NextColumn();
    StatusLine("Clock", true, g_state.clockDriftEnabled, g_state.clockDriftCollector.IsRunning(), g_state.entropyClock);
    ImGui::NextColumn();
    StatusLine("Jitter", FEATURE_CPU_JITTER_IMPLEMENTED, g_state.cpuJitterEnabled, g_state.cpuJitterCollector.IsRunning(), g_state.entropyJitter);
    
    ImGui::Columns(1); // Reset columns
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Keyboard Output Section (Middle)
    ImGui::Text("Keyboard Output:");
    
    // Draw a box around it (visual only, using child window for scrolling if needed)
    ImGui::BeginChild("KeyOutputBox", ImVec2(0, 60), true);
    if (g_state.keystrokePreview.empty()) {
         ImGui::TextDisabled(" [ Keystrokes will appear here ] ");
    } else {
         ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f)); // Matrix Green
         
         // Calculate how many characters fit in the bar
         float availWidth = ImGui::GetContentRegionAvail().x - 10.0f; // Padding
         std::string displayText = g_state.keystrokePreview;
         
         // Trim from left until it fits (scroll effect)
         while (!displayText.empty() && ImGui::CalcTextSize(displayText.c_str()).x > availWidth) {
             displayText = displayText.substr(1);
         }
         
         // Left aligned (normal document flow)
         ImGui::Text("%s", displayText.c_str());
         ImGui::PopStyleColor();
         if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3. Mouse Path Visualization (Bottom)
    ImGui::Text("Mouse Path:");
    
    // Create a canvas for drawing
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize to fit remaining space
    
    // Deduct space for Stop button at bottom (approx 60px to avoid clipping)
    canvas_sz.y -= 60.0f;
    
    if (canvas_sz.y < 100.0f) canvas_sz.y = 100.0f;        // Minimum height
    
    ImGui::InvisibleButton("canvas", canvas_sz);
    
    // VISUALIZATION CAPTURE: Mouse (Canvas Only)
    if (ImGui::IsItemHovered()) {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        // Calculate relative position (0.0 - 1.0)
        float relX = (mouse_pos.x - canvas_p0.x) / canvas_sz.x;
        float relY = (mouse_pos.y - canvas_p0.y) / canvas_sz.y;
        
        // Clamp to ensure 0..1
        if (relX >= 0.0f && relX <= 1.0f && relY >= 0.0f && relY <= 1.0f) {
             // Only add if changed
             static ImVec2 lastRel = ImVec2(-1,-1);
             if (relX != lastRel.x || relY != lastRel.y) {
                 AppState::VizPoint pt;
                 pt.x = relX; pt.y = relY;
                 g_state.mouseTrail.push_back(pt);
                 if (g_state.mouseTrail.size() > 1000) g_state.mouseTrail.erase(g_state.mouseTrail.begin());
                 lastRel = ImVec2(relX, relY);
             }
        }
    }
    
    // Draw border and background
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y), IM_COL32(20, 20, 20, 255));
    draw_list->AddRect(canvas_p0, ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y), IM_COL32(100, 100, 100, 255));

    // Draw Trail
    if (!g_state.mouseTrail.empty()) {
        for (const auto& point : g_state.mouseTrail) {
            // Map normalized coordinates (0..1) to canvas size
            float x = canvas_p0.x + point.x * canvas_sz.x;
            float y = canvas_p0.y + point.y * canvas_sz.y;
            draw_list->AddCircleFilled(ImVec2(x, y), 2.0f, IM_COL32(0, 255, 0, 150));
        }
    } else {
        // Placeholder text centered
        const char* text = "Move mouse to generate trail...";
        ImVec2 textSize = ImGui::CalcTextSize(text);
        draw_list->AddText(ImVec2(canvas_p0.x + (canvas_sz.x - textSize.x) * 0.5f, canvas_p0.y + (canvas_sz.y - textSize.y) * 0.5f), IM_COL32(100, 100, 100, 255), text);
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Stop button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.70f, 0.25f, 0.25f, 1.00f));
    if (ImGui::Button("Stop Collection", ImVec2(180, 0))) {
      g_state.isCollecting = false;
    }
    ImGui::PopStyleColor(2);
  }
  ImGui::End();
}
