#pragma once

#include <cstddef>
#include <climits>

namespace AppConfig {
    // ---------------------------------------------------------
    // Output Limits Configurations
    // ---------------------------------------------------------

    // Decimal Generator Limits
    constexpr int DECIMAL_MIN_DIGITS = 1;
    constexpr int DECIMAL_MAX_DIGITS = INT_MAX;

    // Binary Generator Limits
    constexpr int BINARY_MIN_LENGTH = 1;
    constexpr int BINARY_MAX_LENGTH = INT_MAX;

    // Custom String Generator Limits
    constexpr int CUSTOM_MIN_LENGTH = 1;
    constexpr int CUSTOM_MAX_LENGTH = INT_MAX;

    // Bit/Byte Generator Limits
    constexpr int BITBYTE_MIN_AMOUNT = 1;
    constexpr int BITBYTE_MAX_AMOUNT = INT_MAX;

    // Passphrase Generator Limits
    constexpr int PASSPHRASE_MIN_WORDS = 1;
    constexpr int PASSPHRASE_MAX_WORDS = 32767;

    // Integer Range Bound Limits
    constexpr long long INTEGER_RANGE_MIN = LLONG_MIN;
    constexpr long long INTEGER_RANGE_MAX = LLONG_MAX;

    // ---------------------------------------------------------
    // CSPRNG Logic Allocations
    // ---------------------------------------------------------

    // Integer Entropy Pool Sampling Request Size Block
    constexpr size_t INTEGER_ENTROPY_BLOCK_BYTES = 32;

    // ---------------------------------------------------------
    // Dynamic Buffer Allocations
    // ---------------------------------------------------------

    // Max bounds for application dynamic string buffers
    constexpr size_t PASSPHRASE_SEPARATOR_MAX_BYTES = 16;
    constexpr size_t OTP_MESSAGE_MAX_BYTES = 1024 * 1024; // 1 MB limit
    constexpr size_t OTP_FILEPATH_MAX_BYTES = 512;
}
