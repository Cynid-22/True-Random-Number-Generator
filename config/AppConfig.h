#pragma once

#include <cstddef>
#include <climits>

namespace AppConfig {
    constexpr int DECIMAL_MIN_DIGITS = 1;
    constexpr int DECIMAL_MAX_DIGITS = INT_MAX;

    constexpr int RANDOM_CHARACTERS_MIN_LENGTH = 1;
    constexpr int RANDOM_CHARACTERS_MAX_LENGTH = INT_MAX;

    constexpr int RAW_DATA_MIN_AMOUNT = 1;
    constexpr int RAW_DATA_MAX_AMOUNT = INT_MAX;

    constexpr int OUTPUT_MIN_QUANTITY = 1;
    constexpr int OUTPUT_MAX_QUANTITY = INT_MAX;

    constexpr int UUIDV7_MIN_COUNTER_BITS = 1;
    constexpr int UUIDV7_MAX_COUNTER_BITS = 42;
    constexpr int UUIDV7_WARN_COUNTER_BITS = 32;
    constexpr int UUIDV7_MIN_INCREMENT_BITS = 1;
    constexpr int UUIDV7_MAX_INCREMENT_BITS = 48;
    constexpr int UUIDV7_WARN_INCREMENT_BITS = 12;

    constexpr int ONE_TIME_PAD_MIN_MESSAGE_LENGTH = 1;
    constexpr int ONE_TIME_PAD_MAX_MESSAGE_LENGTH = INT_MAX;

    constexpr int PASSPHRASE_MIN_WORDS = 1;
    constexpr int PASSPHRASE_MAX_WORDS = 32767;

    constexpr long long INTEGER_RANGE_MIN = LLONG_MIN;
    constexpr long long INTEGER_RANGE_MAX = LLONG_MAX;

    constexpr size_t REJECTION_MIN_BYTES_PER_ITEM = 64;
    constexpr unsigned INTEGER_MAX_RETRIES = 4;

    constexpr size_t PASSPHRASE_SEPARATOR_MAX_BYTES = 16;
    constexpr size_t ONE_TIME_PAD_MESSAGE_MAX_BYTES = 1024 * 1024;
    constexpr size_t MAX_UTF8_BYTES_PER_UTF16_CHAR = 3;
    constexpr size_t ONE_TIME_PAD_FILE_PATH_MAX_CHARS = 1024;
    constexpr size_t ONE_TIME_PAD_FILE_PATH_MAX_BYTES =
        ONE_TIME_PAD_FILE_PATH_MAX_CHARS * MAX_UTF8_BYTES_PER_UTF16_CHAR + 1;
}
