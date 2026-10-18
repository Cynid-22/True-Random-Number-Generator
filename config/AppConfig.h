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

    // Custom String Generator Limits
    constexpr int CUSTOM_MIN_LENGTH = 1;
    constexpr int CUSTOM_MAX_LENGTH = INT_MAX;

    // Bit/Byte Generator Limits
    constexpr int BITBYTE_MIN_AMOUNT = 1;
    constexpr int BITBYTE_MAX_AMOUNT = INT_MAX;

    // Quantity Limits (items per generation, all formats except One-Time Pad)
    constexpr int OUTPUT_MIN_QUANTITY = 1;
    constexpr int OUTPUT_MAX_QUANTITY = INT_MAX;

    // UUIDv7 Ordering Limits
    // Method 1 counter: up to 42 bits keeps at least 32 random bits per UUID
    constexpr int UUIDV7_MIN_COUNTER_BITS = 1;
    constexpr int UUIDV7_MAX_COUNTER_BITS = 42;
    // At or above this, the GUI warns that few random bits are left
    constexpr int UUIDV7_WARN_COUNTER_BITS = 32;
    // Method 2 random increment size. 48 is the most the per-UUID random
    // bytes provide; it still leaves room for 2^25+ UUIDs per millisecond.
    constexpr int UUIDV7_MIN_INCREMENT_BITS = 1;
    constexpr int UUIDV7_MAX_INCREMENT_BITS = 48;
    // At or below this, the GUI warns that the next UUID is easy to guess
    constexpr int UUIDV7_WARN_INCREMENT_BITS = 12;

    // Passphrase Generator Limits
    constexpr int PASSPHRASE_MIN_WORDS = 1;
    constexpr int PASSPHRASE_MAX_WORDS = 32767;

    // Integer Range Bound Limits
    constexpr long long INTEGER_RANGE_MIN = LLONG_MIN;
    constexpr long long INTEGER_RANGE_MAX = LLONG_MAX;

    // ---------------------------------------------------------
    // CSPRNG Logic Allocations
    // ---------------------------------------------------------

    // Random bytes per item for formats using rejection sampling (Decimal,
    // Integer, Random Characters): at least this many, so each item keeps the
    // same retry headroom (e.g. 8 integer draws) whatever the quantity.
    constexpr size_t REJECTION_MIN_BYTES_PER_ITEM = 64;

    // Integer rejection sampling: fresh-byte retries when every draw in a
    // block is rejected. Worst case (range just over 2^63) fails all retries
    // with probability ~2^-40.
    constexpr unsigned INTEGER_MAX_RETRIES = 4;

    // ---------------------------------------------------------
    // Dynamic Buffer Allocations
    // ---------------------------------------------------------

    // Max bounds for application dynamic string buffers
    constexpr size_t PASSPHRASE_SEPARATOR_MAX_BYTES = 16;
    constexpr size_t OTP_MESSAGE_MAX_BYTES = 1024 * 1024; // 1 MB limit
    constexpr size_t OTP_FILEPATH_MAX_BYTES = 512;
}
