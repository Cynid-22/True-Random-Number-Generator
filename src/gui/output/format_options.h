#pragma once

inline constexpr char SECURITY_STRENGTH_NOTE[] =
    "A PSEUDO-RANDOM output comes from HMAC_DRBG (SHA-512) alone, whose security\n"
    "strength is 256 bits: above that, counts measure choices, not how hard\n"
    "the output is to guess. A TRUE RANDOM output is XORed with as many\n"
    "true random reserve bits as it draws, so it is not limited to 256 bits\n"
    "if the sources' credit estimates hold.";

void BeginOptionRow(const char *label, bool alignToInput = true);

bool RenderDecimalOptions();
bool RenderIntegerOptions();
bool RenderRandomCharactersOptions();
bool RenderPassphraseOptions();
bool RenderRawDataOptions();
void RenderRawDataCaseToggle();
bool RenderUuidOptions();
void RenderOneTimePadOptions();
