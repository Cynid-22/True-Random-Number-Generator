#pragma once

#include "imgui.h"

void RenderDataLockWarning();

void RenderSourceSeparator();

void RenderSourceToggle(const char *checkboxLabel, bool *enabled,
                        const char *sourceName);

void RenderSourceBadge(const ImVec4 &color, const char *label,
                       const char *tooltipFormat, ...);

void RenderExcludedBadge();
