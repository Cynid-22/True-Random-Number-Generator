#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "../crypto/secure_mem.h"
#include "../../config/AppConfig.h"
#include "output_format.h"

struct OutputSettings {
    OutputSettings() {
        passphraseSeparator.assign(AppConfig::PASSPHRASE_SEPARATOR_MAX_BYTES, '\0');
        passphraseSeparator[0] = '-';
        oneTimePadMessage.assign(AppConfig::ONE_TIME_PAD_MESSAGE_MAX_BYTES, '\0');
        oneTimePadFilePath.assign(AppConfig::ONE_TIME_PAD_FILE_PATH_MAX_BYTES, '\0');
    }

    OutputSettings(const OutputSettings&) = default;
    OutputSettings(OutputSettings&&) = default;
    OutputSettings& operator=(const OutputSettings&) = default;
    OutputSettings& operator=(OutputSettings&&) = default;

    ~OutputSettings() {
        Crypto::SecureClearVector(oneTimePadMessage);
        Crypto::SecureClearVector(oneTimePadFilePath);
        Crypto::SecureClearVector(passphraseSeparator);
    }

    int outputFormat = FORMAT_DECIMAL;

    int outputQuantity = 1;
    bool blankLineBetweenItems = false;
    bool numberListItems = false;

    int decimalDigits = 16;
    long long integerMin = 0;
    long long integerMax = 100;
    int randomCharactersLength = 16;
    bool includeNumbers = true;
    bool includeUppercase = true;
    bool includeLowercase = true;
    bool includeSpecial = false;
    int rawDataAmount = 256;
    int rawDataUnit = RAW_DATA_UNIT_BITS;
    int rawDataEncoding = 0;
    // Indexed by CSPRNG::RawEncoding
    static constexpr int RAW_DATA_ENCODING_COUNT = 14;
    static constexpr std::array<bool, RAW_DATA_ENCODING_COUNT> DEFAULT_RAW_DATA_GROUPING_ENABLED = {false, false, true, false, false, false, false, false, false, false, false, false, false, false};
    static constexpr std::array<int, RAW_DATA_ENCODING_COUNT>  DEFAULT_RAW_DATA_GROUP_SIZE       = {4,     4,     8,    4,     4,     4,     8,     4,     4,     3,     4,     4,     4,     5};
    static constexpr std::array<bool, RAW_DATA_ENCODING_COUNT> DEFAULT_RAW_DATA_UPPERCASE        = {true,  false, false, false, false, false, true,  true,  false, true,  false, false, false, false};
    std::array<bool, RAW_DATA_ENCODING_COUNT> rawDataGroupingEnabled = DEFAULT_RAW_DATA_GROUPING_ENABLED;
    std::array<int, RAW_DATA_ENCODING_COUNT> rawDataGroupSize = DEFAULT_RAW_DATA_GROUP_SIZE;
    std::array<bool, RAW_DATA_ENCODING_COUNT> rawDataUppercase = DEFAULT_RAW_DATA_UPPERCASE;

    bool uuidUppercase = false;
    bool uuidHyphens = true;
    int uuidV7Ordering = UUIDV7_DEDICATED_COUNTER;
    int uuidV7CounterBits = 12;
    int uuidV7IncrementBits = 32;

    int passphraseWordList = WORD_LIST_NORDPASS;
    int passphraseWordCount = 6;
    int passphraseNumberCount = 0;
    std::vector<char> passphraseSeparator;

    std::vector<char> oneTimePadMessage;
    std::vector<char> oneTimePadFilePath;
    long long oneTimePadFileSize = 0;
    uint64_t oneTimePadFileLastWriteTime = 0;
    int oneTimePadMessageLength = 16;
    int oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;
};
