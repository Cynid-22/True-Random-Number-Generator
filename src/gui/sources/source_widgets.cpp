#include "source_widgets.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "../widgets.h"
#include <cstdarg>

void RenderDataLockWarning() {
  if (g_state.entropyPool.IsDataLocked() && g_state.showDataLockWarning) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
    ImGui::TextWrapped("WARNING: Data is locked! Source selection only affects NEW data, which no generation has used yet.");
    ImGui::TextWrapped("Due to security requirements, locked entropy (used by a previous generation) cannot be excluded.");
    ImGui::PopStyleColor();

    if (ImGui::Button("Dismiss Warning")) {
      g_state.showDataLockWarning = false;
    }

    ImGui::Spacing();
    ImGui::Separator();
  }
}

void RenderSourceSeparator() {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
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

void RenderExcludedBadge() {
  RenderSourceBadge(UiColor::DimText, "[Excluded]",
                    "This source is disabled by the user and will not "
                    "contribute to entropy collection.");
}
