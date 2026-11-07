#include "toeplitz.h"
#include "toeplitz_seed.h"
#include <array>
#include <iterator>
#include <windows.h>

namespace Crypto {

namespace {

constexpr size_t BITS_PER_BYTE = 8;
constexpr size_t BITS_PER_WORD = 64;
constexpr size_t BITS_PER_HEX_DIGIT = 4;
constexpr size_t HEX_DIGITS_PER_WORD = BITS_PER_WORD / BITS_PER_HEX_DIGIT;
constexpr size_t WORDS_PER_PULSE = 8;
constexpr size_t PULSE_COUNT = std::size(TOEPLITZ_SEED_PULSES);
constexpr size_t SEED_WORDS = PULSE_COUNT * WORDS_PER_PULSE;
constexpr size_t OUTPUT_WORDS = ToeplitzExtractor::OUTPUT_BITS / BITS_PER_WORD;

static_assert(SEED_WORDS * BITS_PER_WORD >= ToeplitzExtractor::SEED_BITS);
static_assert(SEED_WORDS >= (ToeplitzExtractor::MAX_INPUT_BITS - 1) / BITS_PER_WORD +
                                OUTPUT_WORDS + 1,
              "the last seed window reads one word past its end");

using SeedWords = std::array<uint64_t, SEED_WORDS>;

uint64_t HexDigitValue(char digit) {
    if (digit >= '0' && digit <= '9')
        return static_cast<uint64_t>(digit - '0');
    if (digit >= 'A' && digit <= 'F')
        return static_cast<uint64_t>(digit - 'A' + 10);
    return static_cast<uint64_t>(digit - 'a' + 10);
}

uint64_t ParseHexWord(const char* hex) {
    uint64_t word = 0;
    for (size_t i = 0; i < HEX_DIGITS_PER_WORD; i++)
        word = (word << BITS_PER_HEX_DIGIT) | HexDigitValue(hex[i]);
    return word;
}

const SeedWords& Seed() {
    static const SeedWords words = [] {
        SeedWords parsed{};
        for (size_t pulse = 0; pulse < PULSE_COUNT; pulse++) {
            for (size_t word = 0; word < WORDS_PER_PULSE; word++) {
                parsed[pulse * WORDS_PER_PULSE + word] =
                    ParseHexWord(TOEPLITZ_SEED_PULSES[pulse] + word * HEX_DIGITS_PER_WORD);
            }
        }
        return parsed;
    }();
    return words;
}

void XorSeedWindow(const SeedWords& seed, size_t firstSeedBit, uint64_t mask,
                   uint64_t result[OUTPUT_WORDS]) {
    const size_t firstWord = firstSeedBit / BITS_PER_WORD;
    const size_t shift = firstSeedBit % BITS_PER_WORD;
    for (size_t k = 0; k < OUTPUT_WORDS; k++) {
        const uint64_t high = seed[firstWord + k] << shift;
        const uint64_t low = (seed[firstWord + k + 1] >> 1) >> (BITS_PER_WORD - 1 - shift);
        result[k] ^= (high | low) & mask;
    }
}

}

bool ToeplitzExtractor::Extract(const uint8_t* input, size_t inputLength,
                                uint8_t output[OUTPUT_BYTES]) {
    if (inputLength > MAX_INPUT_BYTES)
        return false;

    const SeedWords& seed = Seed();
    uint64_t result[OUTPUT_WORDS] = {};
    const size_t inputBits = inputLength * BITS_PER_BYTE;
    for (size_t inputBit = 0; inputBit < inputBits; inputBit++) {
        const uint64_t bit =
            (input[inputBit / BITS_PER_BYTE] >> (BITS_PER_BYTE - 1 - inputBit % BITS_PER_BYTE)) & 1;
        XorSeedWindow(seed, MAX_INPUT_BITS - 1 - inputBit, uint64_t(0) - bit, result);
    }

    for (size_t k = 0; k < OUTPUT_WORDS; k++) {
        for (size_t b = 0; b < BITS_PER_WORD / BITS_PER_BYTE; b++) {
            output[k * (BITS_PER_WORD / BITS_PER_BYTE) + b] =
                static_cast<uint8_t>(result[k] >> (BITS_PER_WORD - BITS_PER_BYTE * (b + 1)));
        }
    }
    SecureZeroMemory(result, sizeof(result));
    return true;
}

}
