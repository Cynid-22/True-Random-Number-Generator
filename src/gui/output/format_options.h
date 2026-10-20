#pragma once

inline constexpr char PIPELINE_STATE_NOTE[] =
    "Every output is a ChaCha20 stream set by 352 bits of pipeline state\n"
    "(a 256-bit key and a 96-bit nonce). Counts above 352 bits measure\n"
    "collected entropy or choices, not how hard the output is to guess.";

void BeginOptionRow(const char *label, bool alignToInput = true);

bool RenderDecimalOptions();
bool RenderIntegerOptions();
bool RenderRandomCharactersOptions();
bool RenderPassphraseOptions();
bool RenderRawDataOptions();
void RenderRawDataCaseToggle();
bool RenderUuidOptions();
void RenderOneTimePadOptions();
