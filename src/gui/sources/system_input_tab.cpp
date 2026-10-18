#include "../../core/app_state.h"
#include "../gui.h"
#include "../widgets.h"
#include "imgui.h"
#include "source_widgets.h"

static void RenderClockDriftSource() {
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
  RenderSourceToggle("Include Clock Drift in Final Calculation",
                     &g_state.clockDriftEnabled, "Clock Drift");

  ImGui::SameLine();
  if (!g_state.clockDriftEnabled) {
    RenderExcludedBadge();
  } else if (g_state.isCollecting) {
    if (g_state.clockDriftCollector.IsRunning()) {
      RenderSourceBadge(UiColor::StatusActive, "[Active]",
                        "Clock Drift collector is actively running.\n\n"
                        "Samples collected: %llu\n"
                        "Collection rate: %.0f samples/sec\n"
                        "Collected entropy: %.1f bits\n"
                        "Estimated entropy per sample: ~2 bits\n"
                        "Source is working correctly.",
                        g_state.clockDriftCollector.GetSampleCount(),
                        g_state.clockDriftCollector.GetSamplesPerSecond(),
                        g_state.clockDriftEntropyBits);
      ImGui::Text("    Samples: %llu | Rate: %.0f samples/sec",
                  g_state.clockDriftCollector.GetSampleCount(),
                  g_state.clockDriftCollector.GetSamplesPerSecond());
    } else {
      RenderSourceBadge(UiColor::StatusReady, "[Starting...]",
                        "Clock Drift collector is starting up.\n"
                        "The background thread is initializing.\n"
                        "Collection will begin shortly.");
    }
  } else {
    RenderSourceBadge(
        UiColor::StatusReady, "[Ready]",
        "Clock Drift collection is enabled and ready.\n"
        "The collector will start automatically when collection begins.\n"
        "This source uses CPU cycle counter measurements to extract hardware "
        "entropy.");
  }
  ImGui::Unindent();
}

static void RenderCpuJitterSource() {
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
  RenderSourceToggle("Include CPU Jitter in Final Calculation",
                     &g_state.cpuJitterEnabled, "CPU Jitter");

  ImGui::SameLine();
  if (!g_state.cpuJitterEnabled) {
    RenderExcludedBadge();
  } else if (g_state.isCollecting) {
    RenderSourceBadge(
        UiColor::StatusActive, "[Active]",
        "CPU Jitter race condition is actively running.\n"
        "Collected: %.1f bits\n"
        "Thread race is generating entropy from OS scheduling jitter.\n"
        "Source is working correctly.",
        g_state.cpuJitterEntropyBits);
  } else {
    RenderSourceBadge(
        UiColor::StatusReady, "[Ready]",
        "CPU Jitter collection is enabled and ready.\n"
        "The race condition threads will start automatically when collection "
        "begins.\n"
        "This source exploits OS scheduling unpredictability for entropy.");
  }
  ImGui::Unindent();
}

void RenderSystemInputTab() {
  ImGui::Text("System Input Sources (Hardware/OS Entropy)");
  ImGui::Separator();
  ImGui::Spacing();

  RenderDataLockWarning();

  ImGui::TextWrapped(
      "These sources exploit hardware imperfections and OS scheduling chaos. "
      "They run in parallel to intentionally create CPU 'traffic jams' for "
      "maximum entropy.");
  ImGui::Spacing();

  if (ImGui::CollapsingHeader("Why Parallel Execution?")) {
    ImGui::TextWrapped(
        "Sequential execution is predictable. Parallel execution forces the "
        "CPU to "
        "context-switch, causing cache contention and thermal variations.");
  }

  RenderSourceSeparator();

  RenderClockDriftSource();

  RenderSourceSeparator();

  RenderCpuJitterSource();
}
