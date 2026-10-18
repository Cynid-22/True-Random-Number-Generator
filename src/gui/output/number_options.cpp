#include "format_options.h"
#include "../../core/app_state.h"
#include "../../logging/logger.h"
#include "imgui.h"
#include <algorithm>

bool RenderDecimalOptions() {
  bool changed = false;

  BeginOptionRow("Decimal digits:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputInt("##Digits", &g_state.decimalDigits)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Decimal]: Digits set to %d",
                g_state.decimalDigits);
  }
  g_state.decimalDigits = std::clamp(g_state.decimalDigits,
                                     AppConfig::DECIMAL_MIN_DIGITS,
                                     AppConfig::DECIMAL_MAX_DIGITS);

  return changed;
}

bool RenderIntegerOptions() {
  bool changed = false;

  BeginOptionRow("Minimum:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputScalar("##Min", ImGuiDataType_S64, &g_state.integerMin)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Integer]: Min set to %lld",
                g_state.integerMin);
  }
  g_state.integerMin = std::clamp(g_state.integerMin,
                                  AppConfig::INTEGER_RANGE_MIN,
                                  AppConfig::INTEGER_RANGE_MAX);

  BeginOptionRow("Maximum:");
  ImGui::SetNextItemWidth(230);
  if (ImGui::InputScalar("##Max", ImGuiDataType_S64, &g_state.integerMax)) {
    changed = true;
    Logger::Log(Logger::Level::INFO, "GUI",
                "Output Config [Integer]: Max set to %lld",
                g_state.integerMax);
  }
  g_state.integerMax = std::clamp(g_state.integerMax,
                                  AppConfig::INTEGER_RANGE_MIN,
                                  AppConfig::INTEGER_RANGE_MAX);

  return changed;
}
