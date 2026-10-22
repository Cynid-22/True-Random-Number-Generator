#include "source_widgets.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../widgets.h"
#include <cstdarg>

void RenderUsedDataWarning() {
  if (g_state.entropyPool.HasCarryOver() && g_state.showUsedDataWarning) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    ImGui::TextWrapped("WARNING: Data used by earlier generations is kept only as a hash, which is mixed into every later output. Source selection only affects NEW data, which no generation has used yet.");
    ImGui::TextWrapped("Used data no longer counts toward the entropy bar. Clear wipes it.");
    ImGui::PopStyleColor();

    if (ImGui::Button("Dismiss Warning")) {
      g_state.showUsedDataWarning = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }
}

void RenderSourceToggle(const char *checkboxLabel, bool *enabled,
                        const char *sourceName) {
  ImGui::BeginDisabled(g_state.isCollecting);
  if (ImGui::Checkbox(checkboxLabel, enabled)) {
    Logger::Log(Logger::Level::INFO, "GUI", "%s source toggled: %s",
                sourceName, *enabled ? "ON" : "OFF");
  }
  ImGui::EndDisabled();
}

void RenderSourceBadge(const ImVec4 &color, const char *label,
                       const char *tooltipFormat, ...) {
  ImGui::TextColored(color, "%s", label);
  if (ImGui::IsItemHovered()) {
    va_list args;
    va_start(args, tooltipFormat);
    ImGui::SetTooltipV(tooltipFormat, args);
    va_end(args);
  }
}

static bool HardwareSourcePassedStartup(Entropy::EntropySource source, bool enabled,
                                        bool available) {
  if (!enabled || !available)
    return true;
  return g_state.entropyPool.GetStartupStatus(source).complete &&
         !g_state.entropyPool.HealthTestFailed(source);
}

bool HardwareSourcesPassedStartup() {
  using Entropy::EntropySource;
  return HardwareSourcePassedStartup(EntropySource::Microphone,
                                     g_state.microphoneEnabled,
                                     !g_state.microphoneCollector.HasFailed()) &&
         HardwareSourcePassedStartup(EntropySource::ClockDrift,
                                     g_state.clockDriftEnabled, true) &&
         HardwareSourcePassedStartup(EntropySource::CpuJitter,
                                     g_state.cpuJitterEnabled, true);
}

bool CanFinishStartupEarly(Entropy::EntropySource source) {
  const Entropy::StartupStatus startup =
      g_state.entropyPool.GetStartupStatus(source);
  return !startup.complete && !g_state.entropyPool.HealthTestFailed(source) &&
         startup.samplesTested >= Entropy::EarlyStartupSampleCount(source) &&
         HardwareSourcesPassedStartup();
}

void RenderHealthTestFailureButtons(Entropy::EntropySource source, bool *enabled,
                                    const char *sourceName) {
  ImGui::PushID(static_cast<int>(source));
  if (ImGui::SmallButton("Turn off")) {
    *enabled = false;
    Logger::Log(Logger::Level::INFO, "GUI",
                "%s turned off after a failed health test", sourceName);
  }
  ImGui::SetItemTooltip(
      "Stop this source. Nothing from it is credited or mixed into the seed.\n"
      "Stop collection to tick it again.");
  ImGui::SameLine();
  if (ImGui::SmallButton("Test again")) {
    g_state.entropyPool.RestartSourceStartup(source);
    Logger::Log(Logger::Level::INFO, "GUI",
                "%s start-up test restarted after a failed health test",
                sourceName);
  }
  ImGui::SetItemTooltip(
      "Discard this source's test results and run its full start-up test "
      "again.\nIt earns credit again only once it passes.");
  ImGui::PopID();
}
