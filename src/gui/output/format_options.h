#pragma once

inline constexpr char SECURITY_STRENGTH_NOTE[] =
    "Every output comes from HMAC_DRBG (SHA-512), whose security strength\n"
    "is 256 bits. Counts above 256 bits measure collected entropy or\n"
    "choices, not how hard the output is to guess.";

void BeginOptionRow(const char *label, bool alignToInput = true);

bool RenderDecimalOptions();
bool RenderIntegerOptions();
bool RenderRandomCharactersOptions();
bool RenderPassphraseOptions();
bool RenderRawDataOptions();
void RenderRawDataCaseToggle();
bool RenderUuidOptions();
void RenderOneTimePadOptions();
