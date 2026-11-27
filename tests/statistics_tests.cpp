#include "test_framework.h"
#include "test_data.h"
#include "../src/core/output_format.h"
#include "../src/logic/formats/list_formats.h"
#include "../src/logic/formats/number_formats.h"
#include "../src/logic/formats/one_time_pad_format.h"
#include "../src/logic/formats/raw_data_format.h"
#include "../src/logic/formats/text_formats.h"
#include "../src/logic/formats/uuid_formats.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <map>
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr double CHI_SQUARE_SIGMAS = 10.0;
constexpr double BINOMIAL_SIGMAS = 7.0;
constexpr size_t UINT32_BYTES = 4;
constexpr uint8_t NO_NUMBER_IN_SLOT = 0xFF;
constexpr size_t DIGIT_COUNT = 10;

class ByteSource {
public:
    explicit ByteSource(uint64_t seed) : m_random(seed) {}

    std::vector<uint8_t> Take(size_t count) {
        std::vector<uint8_t> bytes(count);
        for (uint8_t& byte : bytes)
            byte = static_cast<uint8_t>(m_random.Next() >> 56);
        return bytes;
    }

private:
    Tests::SplitMix64 m_random;
};

double ChiSquare(const std::vector<size_t>& counts, const std::vector<double>& expectedCounts) {
    double sum = 0.0;
    for (size_t i = 0; i < counts.size(); i++) {
        const double difference = static_cast<double>(counts[i]) - expectedCounts[i];
        sum += difference * difference / expectedCounts[i];
    }
    return sum;
}

bool MatchesExpectedCounts(const std::vector<size_t>& counts,
                           const std::vector<double>& expectedCounts) {
    const double degreesOfFreedom = static_cast<double>(counts.size()) - 1.0;
    return ChiSquare(counts, expectedCounts) <=
           degreesOfFreedom + CHI_SQUARE_SIGMAS * std::sqrt(2.0 * degreesOfFreedom);
}

bool LooksUniform(const std::vector<size_t>& counts) {
    const size_t total = std::accumulate(counts.begin(), counts.end(), size_t(0));
    const double expectedCount = static_cast<double>(total) / static_cast<double>(counts.size());
    return MatchesExpectedCounts(counts, std::vector<double>(counts.size(), expectedCount));
}

bool NearExpected(size_t count, size_t trials, double probability) {
    const double expected = static_cast<double>(trials) * probability;
    const double sigma = std::sqrt(static_cast<double>(trials) * probability * (1.0 - probability));
    return std::fabs(static_cast<double>(count) - expected) <= BINOMIAL_SIGMAS * sigma;
}

uint32_t LittleEndianUint32(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    for (size_t b = 0; b < UINT32_BYTES; b++)
        value |= static_cast<uint32_t>(bytes[offset + b]) << (8 * b);
    return value;
}

std::string UuidTextOf(const uint8_t bytes[16]) {
    const std::string hex = Tests::BytesToHex(bytes, 16);
    return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" +
           hex.substr(16, 4) + "-" + hex.substr(20, 12);
}

struct DigitEncoding {
    int encoding;
    size_t radix;
};

const DigitEncoding DIGIT_ENCODINGS[] = {
    {CSPRNG::RAW_DECIMAL, 10}, {CSPRNG::RAW_DUODECIMAL, 12}, {CSPRNG::RAW_BASE36, 36},
    {CSPRNG::RAW_BASE58, 58},  {CSPRNG::RAW_BASE62, 62},
};

}

TEST(StatisticsHelperFlagsAMissingValue) {
    CHECK(LooksUniform({1000, 1003, 998}));
    CHECK(!LooksUniform({1000, 1000, 0}));
    CHECK(!LooksUniform({1500, 1000, 500}));
    CHECK(NearExpected(500, 1000, 0.5));
    CHECK(!NearExpected(700, 1000, 0.5));
}

TEST(ListPicksAreUniformAcrossItems) {
    const size_t ITEM_COUNT = 7;
    const size_t PICK_COUNT = 70000;
    ByteSource bytes(101);
    const std::vector<size_t> picks =
        CSPRNG::PickListItems(bytes.Take(CSPRNG::ListPickByteCount(static_cast<int>(PICK_COUNT))),
                              std::vector<uint64_t>(ITEM_COUNT, 1), PICK_COUNT, true);
    std::vector<size_t> counts(ITEM_COUNT, 0);
    for (size_t pick : picks)
        counts[pick]++;
    CHECK(LooksUniform(counts));
}

TEST(WeightedPicksFollowTheWeights) {
    const std::vector<uint64_t> weights = {1, 2, 3, 4};
    const size_t PICK_COUNT = 100000;
    ByteSource bytes(102);
    const std::vector<size_t> picks = CSPRNG::PickListItems(
        bytes.Take(CSPRNG::ListPickByteCount(static_cast<int>(PICK_COUNT))), weights, PICK_COUNT,
        true);
    std::vector<size_t> counts(weights.size(), 0);
    for (size_t pick : picks)
        counts[pick]++;
    CHECK(MatchesExpectedCounts(counts, {10000.0, 20000.0, 30000.0, 40000.0}));
}

TEST(PicksWithoutRepeatsFollowSequentialWeights) {
    const std::vector<uint64_t> weights = {1, 1, 2};
    const size_t TRIALS = 30000;
    ByteSource bytes(103);
    std::vector<size_t> firstPickCounts(weights.size(), 0);
    std::vector<size_t> inclusionCounts(weights.size(), 0);
    size_t repeatedPicks = 0;
    for (size_t trial = 0; trial < TRIALS; trial++) {
        const std::vector<size_t> picks =
            CSPRNG::PickListItems(bytes.Take(CSPRNG::ListPickByteCount(2)), weights, 2, false);
        firstPickCounts[picks[0]]++;
        inclusionCounts[picks[0]]++;
        inclusionCounts[picks[1]]++;
        if (picks[0] == picks[1])
            repeatedPicks++;
    }
    CHECK_EQ(repeatedPicks, size_t(0));
    CHECK(MatchesExpectedCounts(firstPickCounts, {7500.0, 7500.0, 15000.0}));
    CHECK(NearExpected(inclusionCounts[0], TRIALS, 7.0 / 12.0));
    CHECK(NearExpected(inclusionCounts[1], TRIALS, 7.0 / 12.0));
    CHECK(NearExpected(inclusionCounts[2], TRIALS, 5.0 / 6.0));
}

TEST(ShufflePermutationsAreEquallyLikely) {
    const size_t ITEM_COUNT = 3;
    const size_t TRIALS = 30000;
    ByteSource bytes(104);
    std::map<std::vector<size_t>, size_t> permutationCounts;
    for (size_t trial = 0; trial < TRIALS; trial++) {
        std::vector<size_t> order(ITEM_COUNT);
        std::iota(order.begin(), order.end(), size_t(0));
        CSPRNG::ShuffleListOrder(bytes.Take(CSPRNG::ListShuffleByteCount(ITEM_COUNT)), order);
        permutationCounts[order]++;
    }
    CHECK_EQ(permutationCounts.size(), size_t(6));
    std::vector<size_t> counts;
    for (const auto& entry : permutationCounts)
        counts.push_back(entry.second);
    CHECK(LooksUniform(counts));
}

TEST(ShuffleSendsEveryItemToEveryPositionEqually) {
    const size_t ITEM_COUNT = 5;
    const size_t TRIALS = 25000;
    ByteSource bytes(105);
    std::vector<std::vector<size_t>> positionCounts(ITEM_COUNT, std::vector<size_t>(ITEM_COUNT, 0));
    for (size_t trial = 0; trial < TRIALS; trial++) {
        std::vector<size_t> order(ITEM_COUNT);
        std::iota(order.begin(), order.end(), size_t(0));
        CSPRNG::ShuffleListOrder(bytes.Take(CSPRNG::ListShuffleByteCount(ITEM_COUNT)), order);
        for (size_t position = 0; position < ITEM_COUNT; position++)
            positionCounts[order[position]][position]++;
    }
    for (const std::vector<size_t>& counts : positionCounts)
        CHECK(LooksUniform(counts));
}

TEST(IntegerValuesAreUniform) {
    const int64_t LOW = -3;
    const int64_t HIGH = 3;
    const size_t DRAWS = 70000;
    ByteSource bytes(106);
    std::vector<size_t> counts(static_cast<size_t>(HIGH - LOW + 1), 0);
    size_t outOfRange = 0;
    for (size_t draw = 0; draw < DRAWS; draw++) {
        std::vector<uint8_t> binaryValue;
        bool allDrawsRejected = false;
        const std::string text = Tests::TextOf(
            CSPRNG::GenerateInteger(bytes.Take(16), LOW, HIGH, allDrawsRejected, binaryValue));
        const long long value = std::stoll(text);
        if (value < LOW || value > HIGH)
            outOfRange++;
        else
            counts[static_cast<size_t>(value - LOW)]++;
    }
    CHECK_EQ(outOfRange, size_t(0));
    CHECK(LooksUniform(counts));
}

TEST(IntegerEdgeRangesStayInBounds) {
    struct Range {
        long long low;
        long long high;
    };
    const Range ranges[] = {{LLONG_MIN, LLONG_MIN + 2}, {LLONG_MAX - 2, LLONG_MAX}, {-1, 1}};
    ByteSource bytes(107);
    for (const Range& range : ranges) {
        std::vector<size_t> counts(3, 0);
        size_t outOfRange = 0;
        for (int draw = 0; draw < 3000; draw++) {
            std::vector<uint8_t> binaryValue;
            bool allDrawsRejected = false;
            const long long value = std::stoll(Tests::TextOf(CSPRNG::GenerateInteger(
                bytes.Take(16), range.low, range.high, allDrawsRejected, binaryValue)));
            if (value < range.low || value > range.high)
                outOfRange++;
            else
                counts[static_cast<size_t>(static_cast<unsigned long long>(value) -
                                           static_cast<unsigned long long>(range.low))]++;
        }
        CHECK_EQ(outOfRange, size_t(0));
        CHECK(LooksUniform(counts));
    }
}

TEST(DecimalDigitsAreUniform) {
    const int DIGITS = 50000;
    ByteSource bytes(108);
    std::vector<uint8_t> binaryValue;
    const std::string decimal = Tests::TextOf(
        CSPRNG::GenerateDecimal(bytes.Take(static_cast<size_t>(DIGITS) * 4), DIGITS, binaryValue));
    CHECK_EQ(decimal.size(), static_cast<size_t>(DIGITS) + 2);
    CHECK_EQ(decimal.substr(0, 2), std::string("0."));
    std::vector<size_t> counts(DIGIT_COUNT, 0);
    for (size_t i = 2; i < decimal.size(); i++)
        counts[static_cast<size_t>(decimal[i] - '0')]++;
    CHECK(LooksUniform(counts));
    CHECK_EQ(binaryValue.size(), static_cast<size_t>(DIGITS));
}

TEST(RandomCharactersAreUniformOverTheSet) {
    const size_t CHARACTER_SET_SIZE = 94;
    const int LENGTH = static_cast<int>(CHARACTER_SET_SIZE) * 500;
    ByteSource bytes(109);
    std::vector<uint8_t> binaryValue;
    const std::string text = Tests::TextOf(CSPRNG::GenerateRandomCharacters(
        bytes.Take(static_cast<size_t>(LENGTH) * 4), LENGTH, true, true, true, true, binaryValue));
    CHECK_EQ(text.size(), static_cast<size_t>(LENGTH));
    std::vector<size_t> counts(CHARACTER_SET_SIZE, 0);
    size_t badIndices = 0;
    for (uint8_t index : binaryValue) {
        if (index >= CHARACTER_SET_SIZE)
            badIndices++;
        else
            counts[index]++;
    }
    CHECK_EQ(badIndices, size_t(0));
    CHECK(LooksUniform(counts));

    const std::string lettersOnly = Tests::TextOf(CSPRNG::GenerateRandomCharacters(
        bytes.Take(4000), 1000, false, true, true, false, binaryValue));
    CHECK_EQ(lettersOnly.size(), size_t(1000));
    CHECK(std::all_of(lettersOnly.begin(), lettersOnly.end(), [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
    }));
}

TEST(PassphraseWordsAreUniform) {
    const std::vector<std::string> words = {"alpha", "bravo", "charlie", "delta", "echo"};
    const int WORD_COUNT = 20000;
    ByteSource bytes(110);
    std::vector<uint8_t> binaryValue;
    const std::string passphrase = Tests::TextOf(CSPRNG::GeneratePassphrase(
        bytes.Take(static_cast<size_t>(WORD_COUNT) * 3 + 300), WORD_COUNT, 0, " ", words,
        binaryValue));
    CHECK_EQ(binaryValue.size(), static_cast<size_t>(WORD_COUNT) * UINT32_BYTES);
    std::vector<size_t> counts(words.size(), 0);
    size_t badIndices = 0;
    for (size_t offset = 0; offset + UINT32_BYTES <= binaryValue.size(); offset += UINT32_BYTES) {
        const uint32_t index = LittleEndianUint32(binaryValue, offset);
        if (index >= words.size())
            badIndices++;
        else
            counts[index]++;
    }
    CHECK_EQ(badIndices, size_t(0));
    CHECK(LooksUniform(counts));
    CHECK_EQ(static_cast<size_t>(std::count(passphrase.begin(), passphrase.end(), ' ')),
             static_cast<size_t>(WORD_COUNT - 1));
}

TEST(PassphraseNumbersAreUniformDigitsInFreeSlots) {
    const std::vector<std::string> words = {"alpha", "bravo", "charlie"};
    const int WORD_COUNT = 1000;
    const int NUMBER_COUNT = 1000;
    ByteSource bytes(111);
    std::vector<uint8_t> binaryValue;
    const std::string passphrase = Tests::TextOf(CSPRNG::GeneratePassphrase(
        bytes.Take(9600), WORD_COUNT, NUMBER_COUNT, "-", words, binaryValue));

    const size_t slotStart = static_cast<size_t>(WORD_COUNT) * UINT32_BYTES;
    const size_t slotCount = static_cast<size_t>(WORD_COUNT) * 2;
    CHECK_EQ(binaryValue.size(), slotStart + slotCount);
    std::vector<size_t> digitCounts(DIGIT_COUNT, 0);
    size_t numbersBeforeWords = 0;
    size_t numbersFound = 0;
    for (size_t slot = 0; slot < slotCount && slotStart + slot < binaryValue.size(); slot++) {
        const uint8_t value = binaryValue[slotStart + slot];
        if (value == NO_NUMBER_IN_SLOT)
            continue;
        numbersFound++;
        if (value < DIGIT_COUNT)
            digitCounts[value]++;
        if (slot % 2 == 0)
            numbersBeforeWords++;
    }
    CHECK_EQ(numbersFound, static_cast<size_t>(NUMBER_COUNT));
    CHECK(LooksUniform(digitCounts));
    CHECK(NearExpected(numbersBeforeWords, static_cast<size_t>(NUMBER_COUNT), 0.5));

    const size_t digitsInText = static_cast<size_t>(std::count_if(
        passphrase.begin(), passphrase.end(),
        [](char character) { return character >= '0' && character <= '9'; }));
    CHECK_EQ(digitsInText, static_cast<size_t>(NUMBER_COUNT));
}

TEST(OneTimePadKeyCharactersAreUniform) {
    const size_t LENGTH = static_cast<size_t>(CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE) * 400;
    ByteSource bytes(112);
    std::vector<char> key;
    CHECK(CSPRNG::GenerateOneTimePadKey(bytes.Take(LENGTH * 2), LENGTH, key));
    CHECK_EQ(key.size(), LENGTH);
    std::vector<size_t> counts(static_cast<size_t>(CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE), 0);
    size_t foreignCharacters = 0;
    for (char character : key) {
        const char* found = character == '\0' ? nullptr
                                              : strchr(CSPRNG::ONE_TIME_PAD_ALPHABET, character);
        if (!found)
            foreignCharacters++;
        else
            counts[static_cast<size_t>(found - CSPRNG::ONE_TIME_PAD_ALPHABET)]++;
    }
    CHECK_EQ(foreignCharacters, size_t(0));
    CHECK(LooksUniform(counts));
}

TEST(RawDataDigitsAreUniformInEveryRadix) {
    const uint64_t BITS = 40000;
    ByteSource bytes(113);
    for (const DigitEncoding& digits : DIGIT_ENCODINGS) {
        const CSPRNG::RawDataLayout layout = CSPRNG::GetRawDataLayout(digits.encoding, BITS);
        CHECK(layout.usesRandomDigits);
        std::vector<uint8_t> binaryValue;
        const std::string text = Tests::TextOf(CSPRNG::GenerateRawData(
            bytes.Take(static_cast<size_t>(layout.randomByteCount)), static_cast<int>(BITS),
            RAW_DATA_UNIT_BITS, digits.encoding, 0, true, binaryValue));
        CHECK_EQ(text.size(), static_cast<size_t>(layout.characterCount));
        CHECK_EQ(binaryValue.size(), static_cast<size_t>(layout.characterCount));
        CHECK(layout.bitsCarried >= static_cast<double>(BITS));
        std::vector<size_t> counts(digits.radix, 0);
        size_t badDigits = 0;
        for (uint8_t value : binaryValue) {
            if (value >= digits.radix)
                badDigits++;
            else
                counts[value]++;
        }
        CHECK_EQ(badDigits, size_t(0));
        CHECK(LooksUniform(counts));
    }
}

TEST(RawDataHexAndBinaryCarryEveryInputBit) {
    ByteSource bytes(114);
    size_t mismatches = 0;
    for (int sample = 0; sample < 200; sample++) {
        const std::vector<uint8_t> input = bytes.Take(32);
        std::vector<uint8_t> binaryValue;
        const std::string hex = Tests::TextOf(CSPRNG::GenerateRawData(
            input, 32, RAW_DATA_UNIT_BYTES, CSPRNG::RAW_HEX, 0, false, binaryValue));
        if (hex != Tests::BytesToHex(input.data(), input.size()) || binaryValue != input)
            mismatches++;

        const std::string binary = Tests::TextOf(CSPRNG::GenerateRawData(
            input, 256, RAW_DATA_UNIT_BITS, CSPRNG::RAW_BINARY, 0, true, binaryValue));
        std::string expectedBinary;
        for (uint8_t byte : input) {
            for (int bit = 7; bit >= 0; bit--)
                expectedBinary.push_back(((byte >> bit) & 1) ? '1' : '0');
        }
        if (binary != expectedBinary)
            mismatches++;
    }
    CHECK_EQ(mismatches, size_t(0));
}

TEST(RawDataPartialBitsAreMaskedInTheBinaryValue) {
    const std::vector<uint8_t> input = {0xFF, 0xFF};
    std::vector<uint8_t> binaryValue;
    const std::string binary = Tests::TextOf(CSPRNG::GenerateRawData(
        input, 10, RAW_DATA_UNIT_BITS, CSPRNG::RAW_BINARY, 0, true, binaryValue));
    CHECK_EQ(binary, std::string("1111111111"));
    CHECK(binaryValue == (std::vector<uint8_t>{0xFF, 0xC0}));

    const std::string octal = Tests::TextOf(CSPRNG::GenerateRawData(
        input, 10, RAW_DATA_UNIT_BITS, CSPRNG::RAW_OCTAL, 0, true, binaryValue));
    CHECK_EQ(octal, std::string("7777"));
    CHECK(binaryValue == (std::vector<uint8_t>{0xFF, 0xF0}));
}

TEST(UuidV4KeepsEveryRandomBit) {
    ByteSource bytes(115);
    size_t mismatches = 0;
    for (int sample = 0; sample < 200; sample++) {
        const std::vector<uint8_t> input = bytes.Take(16);
        uint8_t expected[16];
        std::copy(input.begin(), input.end(), expected);
        expected[6] = static_cast<uint8_t>((expected[6] & 0x0F) | 0x40);
        expected[8] = static_cast<uint8_t>((expected[8] & 0x3F) | 0x80);
        std::vector<uint8_t> binaryValue;
        const std::string uuid =
            Tests::TextOf(CSPRNG::GenerateUUIDv4(input, false, true, binaryValue));
        if (uuid != UuidTextOf(expected) ||
            binaryValue != std::vector<uint8_t>(expected, expected + 16))
            mismatches++;
    }
    CHECK_EQ(mismatches, size_t(0));
}
