#pragma once

#include "imgui.h"

namespace UiColor {
inline constexpr ImVec4 DimText(0.5f, 0.5f, 0.5f, 1.0f);
inline constexpr ImVec4 LabelText(0.6f, 0.6f, 0.6f, 1.0f);
inline constexpr ImVec4 Warning(1.0f, 0.6f, 0.0f, 1.0f);
inline constexpr ImVec4 Highlight(0.3f, 0.8f, 1.0f, 1.0f);
inline constexpr ImVec4 StatusActive(0.3f, 1.0f, 0.5f, 1.0f);
inline constexpr ImVec4 StatusReady(1.0f, 0.8f, 0.3f, 1.0f);
inline constexpr ImVec4 StatusUnavailable(1.0f, 0.3f, 0.3f, 1.0f);
inline constexpr ImVec4 StopButton(0.60f, 0.20f, 0.20f, 1.00f);
inline constexpr ImVec4 StopButtonHovered(0.70f, 0.25f, 0.25f, 1.00f);
}

float ComboWidthForText(float textWidth);

void InlineDivider();

void HelpMarker(const char *text);

const char *CurrentSpinnerFrame();
