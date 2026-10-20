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
