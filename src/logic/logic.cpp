#include "logic.h"
#include "formats/one_time_pad_format.h"
#include "formats/raw_data_format.h"
#include "formats/text_formats.h"
#include "word_lists.h"
#include "../core/app_state.h"
#include <cmath>
#include <cstdint>
#include <cstring>

float CalculateRequiredEntropy() {
    float bits = 0.0f;

    switch (g_state.outputFormat) {
        case FORMAT_DECIMAL:
            bits = g_state.decimalDigits * 3.321928f;
            break;

        case FORMAT_INTEGER:
        {
            bool swapped = g_state.integerMin > g_state.integerMax;
            long long low = swapped ? g_state.integerMax : g_state.integerMin;
            long long high = swapped ? g_state.integerMin : g_state.integerMax;
            unsigned long long range = (unsigned long long)high - (unsigned long long)low + 1;
            if (range == 0) bits = 64.0f;
            else if (range <= 1) bits = 1.0f;
            else bits = std::ceil(std::log2((double)range));
            break;
        }

        case FORMAT_RANDOM_CHARACTERS:
        {
            int characterSetSize = 0;
            if (g_state.includeNumbers) characterSetSize += 10;
            if (g_state.includeUppercase) characterSetSize += 26;
            if (g_state.includeLowercase) characterSetSize += 26;
            if (g_state.includeSpecial) characterSetSize += 32;

            if (characterSetSize < 1) characterSetSize = 1;
            bits = g_state.randomCharactersLength * std::log2((double)characterSetSize);
            break;
        }

        case FORMAT_RAW_DATA:
            bits = (float)CSPRNG::GetRawDataLayout(
                       g_state.rawDataEncoding,
                       CSPRNG::RawDataBitCount(g_state.rawDataAmount, g_state.rawDataUnit))
                       .bitsCarried;
            break;

        case FORMAT_PASSPHRASE:
            bits = (float)(g_state.passphraseWordCount *
                               WordLists::Get(g_state.passphraseWordList).bitsPerWord +
                           CSPRNG::PassphraseNumberBits(g_state.passphraseWordCount,
                                                        g_state.passphraseNumberCount));
            break;

        case FORMAT_ONE_TIME_PAD:
            if (g_state.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT) {
                bits = strlen(g_state.oneTimePadMessage.data()) *
                       std::log2((double)CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE);
            } else if (g_state.oneTimePadInputMode == ONE_TIME_PAD_MESSAGE_LENGTH_INPUT) {
                bits = g_state.oneTimePadMessageLength *
                       std::log2((double)CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE);
            } else {
                bits = (float)g_state.oneTimePadFileSize * 8.0f;
            }
            break;

        case FORMAT_UUID_V4:
            bits = 122.0f;
            break;

        case FORMAT_UUID_V7:
            bits = (g_state.uuidV7Ordering == UUIDV7_FULLY_RANDOM) ? 74.0f : 73.0f;
            break;
    }

    if (g_state.outputFormat != FORMAT_ONE_TIME_PAD && g_state.outputQuantity > 1)
        bits *= (float)g_state.outputQuantity;

    return (bits < AppConfig::MINIMUM_ENTROPY_BITS) ? AppConfig::MINIMUM_ENTROPY_BITS : bits;
}

void UpdateTargetEntropy() {
    g_state.targetEntropyBits = CalculateRequiredEntropy();
}
