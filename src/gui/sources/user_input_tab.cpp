#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"

static const char *UNAVAILABLE_MICROPHONE_TOOLTIP =
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
    "drivers, etc.) to resume collection.";

static void RenderMicrophoneSource() {
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
  RenderSourceToggle("Include Microphone in Final Calculation",
                     &g_state.microphoneEnabled, "Microphone");

  ImGui::SameLine();
  if (!g_state.microphoneEnabled) {
    RenderExcludedBadge();
  } else if (g_state.isCollecting) {
    if (g_state.microphoneCollector.IsRunning()) {
      RenderSourceBadge(UiColor::StatusActive, "[Active]",
                        "Microphone is actively collecting entropy.\n"
                        "Collected: %.1f bits\n"
                        "Source is working correctly.",
                        g_state.microphoneEntropyBits);
    } else {
      RenderSourceBadge(UiColor::StatusUnavailable, "[UNAVAILABLE]",
                        UNAVAILABLE_MICROPHONE_TOOLTIP);
    }
  } else {
    RenderSourceBadge(UiColor::StatusReady, "[Ready]",
                      "Microphone is enabled and ready to collect data when "
                      "collection starts.");
  }
  ImGui::Unindent();
}

static void RenderKeystrokeSource() {
  ImGui::Text("Keystroke Dynamics (User Entropy)");
  ImGui::Indent();
  if (ImGui::CollapsingHeader("How it works##key")) {
    ImGui::TextWrapped(
        "Captures your unique typing rhythm: Flight Time (gap between keys) "
        "and Dwell Time (how long each key is held). These timings are "
        "measured "
        "in nanoseconds and are unique to each person.");
  }
  RenderSourceToggle("Include Keystroke in Final Calculation",
                     &g_state.keystrokeEnabled, "Keystroke");

  ImGui::SameLine();
  if (!g_state.keystrokeEnabled) {
    RenderExcludedBadge();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(UiColor::StatusActive, "[Active]",
                      "Keystroke dynamics are actively being collected.\n"
                      "Collected: %.1f bits\n"
                      "Source is working correctly.",
                      g_state.keystrokeEntropyBits);
  } else {
    RenderSourceBadge(UiColor::StatusReady, "[Ready]",
                      "Keystroke capture is enabled and ready to collect "
                      "data when collection starts.");
  }
  ImGui::Unindent();
}

static void RenderMouseSource() {
  ImGui::Text("Mouse Movement (User Entropy)");
  ImGui::Indent();
  if (ImGui::CollapsingHeader("How it works##mouse")) {
    ImGui::TextWrapped(
        "Records X/Y coordinates and precise timestamps (nanoseconds) of mouse "
        "movements. "
        "Your physical motor noise creates unpredictable patterns. "
        "Small movements (<3 pixels) are filtered to avoid sensor drift.");
  }
  RenderSourceToggle("Include Mouse in Final Calculation",
                     &g_state.mouseMovementEnabled, "Mouse");

  ImGui::SameLine();
  if (!g_state.mouseMovementEnabled) {
    RenderExcludedBadge();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(UiColor::StatusActive, "[Active]",
                      "Mouse movement is actively being tracked.\n"
                      "Collected: %.1f bits\n"
                      "Source is working correctly.",
                      g_state.mouseEntropyBits);
  } else {
    RenderSourceBadge(UiColor::StatusReady, "[Ready]",
                      "Mouse movement tracking is enabled and ready to "
                      "collect data when collection starts.");
  }
  ImGui::Unindent();
}

void RenderUserInputTab() {
  ImGui::Text("User Input Sources");
  ImGui::Separator();
  ImGui::Spacing();

  RenderDataLockWarning();

  ImGui::TextWrapped("Configure which user-generated entropy sources to "
                     "include in the final calculation.");
  ImGui::Spacing();
  ImGui::Spacing();

  RenderMicrophoneSource();

  RenderSourceSeparator();

  RenderKeystrokeSource();

  RenderSourceSeparator();

  RenderMouseSource();
}
