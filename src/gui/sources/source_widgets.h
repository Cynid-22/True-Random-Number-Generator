#pragma once

#include "imgui.h"

inline constexpr float INPUT_COLUMN_GAP = 24.0f;

void RenderDataLockWarning();

void RenderSourceToggle(const char *checkboxLabel, bool *enabled,
                        const char *sourceName);

void RenderSourceBadge(const ImVec4 &color, const char *label,
                       const char *tooltipFormat, ...);

void RenderCollectionPad();
