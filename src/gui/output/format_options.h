#pragma once

void BeginOptionRow(const char *label, bool alignToInput = true);

bool RenderDecimalOptions();
bool RenderIntegerOptions();
bool RenderRandomCharactersOptions();
bool RenderPassphraseOptions();
bool RenderRawDataOptions();
void RenderRawDataCaseToggle();
bool RenderUuidOptions();
void RenderOneTimePadOptions();
