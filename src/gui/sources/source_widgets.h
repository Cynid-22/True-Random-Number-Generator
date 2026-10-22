#pragma once

#include "imgui.h"
#include "../../entropy/entropy_common.h"

inline constexpr float INPUT_COLUMN_GAP = 24.0f;

void RenderUsedDataWarning();

void RenderSourceToggle(const char *checkboxLabel, bool *enabled,
                        const char *sourceName);

void RenderSourceBadge(const ImVec4 &color, const char *label,
                       const char *tooltipFormat, ...);

bool HardwareSourcesPassedStartup();

bool CanFinishStartupEarly(Entropy::EntropySource source);

void RenderHealthTestFailureButtons(Entropy::EntropySource source, bool *enabled,
                                    const char *sourceName);

void RenderCollectionPad();
