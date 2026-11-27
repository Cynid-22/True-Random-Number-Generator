#include "test_framework.h"
#include "test_data.h"
#include "../src/core/app_state.h"
#include "../src/logic/formats/list_formats.h"
#include "../src/logic/formats/number_formats.h"
#include "../src/logic/formats/one_time_pad_format.h"
#include "../src/logic/formats/raw_data_format.h"
#include "../src/logic/formats/uuid_formats.h"
#include "../src/logic/list_input.h"
#include "../src/logic/logic.h"
#include "../src/logic/output_export.h"
#include "../src/logic/output_generator.h"
#include "../src/logic/word_lists.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using CSPRNG::GenerationMode;
using CSPRNG::GenerationResult;
using Entropy::EntropySource;
using Tests::MICROPHONE_ONLY;
using Tests::TextOf;

namespace {

constexpr double FULL_STARTUP_RESERVE_BITS = 12 * 512.0;
constexpr double BITS_PER_BYTE = 8.0;
constexpr double RELATIVE_TOLERANCE = 1e-4;
constexpr size_t UUID_TEXT_LENGTH = 36;
constexpr size_t INTEGER_BINARY_BYTES = 8;
constexpr size_t LIST_INDEX_BYTES = 4;
constexpr int OTP_ALPHABET_SIZE = CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE;

Entropy::EntropyPool& Pool() {
    return g_state.entropyPool;
}

void FeedFullMicrophoneStartup() {
    Pool().SecureWipe();
    Pool().AddDataPoints(Tests::SamplePoints(EntropySource::Microphone,
                                             Tests::FIRST_FULL_CREDIT_SEED, 0,
                                             Entropy::STARTUP_SAMPLE_COUNT),
                         MICROPHONE_ONLY);
}

void SetBuffer(std::vector<char>& buffer, const std::string& text) {
    std::fill(buffer.begin(), buffer.end(), '\0');
    std::copy(text.begin(), text.end(), buffer.begin());
}

void SetWeight(ListItemWeights& weights, const char* item, int weight) {
    weights.emplace(std::piecewise_construct, std::forward_as_tuple(item),
                    std::forward_as_tuple(weight));
}

std::string TextBeforeNull(const std::vector<char>& text) {
    return std::string(text.begin(), std::find(text.begin(), text.end(), '\0'));
}

std::vector<std::string> SplitAt(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        const size_t end = text.find(delimiter, start);
        parts.push_back(text.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos)
            return parts;
        start = end + 1;
    }
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

bool IsDigit(char character) {
    return character >= '0' && character <= '9';
}

bool AllDigits(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), IsDigit);
}

bool CloseTo(double actual, double expected) {
    return std::fabs(actual - expected) <= RELATIVE_TOLERANCE * std::max(1.0, std::fabs(expected));
}

int OneTimePadIndex(char character) {
    const char* found =
        character == '\0' ? nullptr : strchr(CSPRNG::ONE_TIME_PAD_ALPHABET, character);
    return found ? static_cast<int>(found - CSPRNG::ONE_TIME_PAD_ALPHABET) : -1;
}

uint32_t LittleEndianUint32(const std::vector<uint8_t>& bytes, size_t offset) {
    uint32_t value = 0;
    for (size_t b = 0; b < LIST_INDEX_BYTES; b++)
        value |= static_cast<uint32_t>(bytes[offset + b]) << (8 * b);
    return value;
}

int64_t LittleEndianInt64(const std::vector<uint8_t>& bytes, size_t offset) {
    uint64_t value = 0;
    for (size_t b = 0; b < INTEGER_BINARY_BYTES; b++)
        value |= static_cast<uint64_t>(bytes[offset + b]) << (8 * b);
    return static_cast<int64_t>(value);
}

OutputSettings ListSettings(int format, const std::string& text) {
    OutputSettings settings;
    settings.outputFormat = format;
    SetBuffer(settings.listText, text);
    return settings;
}

std::string Csv(const GeneratedOutputDetails& details, const std::string& text) {
    return TextOf(BuildOutputCsv(details, text.data(), text.size()));
}

std::string Json(const GeneratedOutputDetails& details, const std::string& text) {
    return TextOf(BuildOutputJson(details, "2026-11-27 12:00:00", text.data(), text.size()));
}

std::string NumberedListText(size_t itemCount) {
    std::string text;
    for (size_t i = 0; i < itemCount; i++)
        text += "item" + std::to_string(i) + "\n";
    return text;
}

void ResetOutputSettingsInAppState() {
    g_state.outputFormat = FORMAT_DECIMAL;
    g_state.outputQuantity = 1;
    g_state.decimalDigits = 16;
    g_state.randomCharactersLength = 16;
    g_state.includeNumbers = true;
    g_state.includeUppercase = true;
    g_state.includeLowercase = true;
    g_state.includeSpecial = false;
    g_state.oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;
    g_state.oneTimePadMessageLength = 16;
    SetBuffer(g_state.listText, "");
    SetBuffer(g_state.listSeparator, ", ");
    g_state.listOneItemPerLine = true;
    g_state.listAllowRepeats = false;
    g_state.listRemovePicked = false;
    g_state.listUseWeights = false;
    g_state.listItemWeights.clear();
}

const OutputSetting* FindSetting(const std::vector<OutputSetting>& settings, const char* name) {
    for (const OutputSetting& setting : settings) {
        if (setting.name == name)
            return &setting;
    }
    return nullptr;
}

}

TEST(DecimalOutputIsLaidOutWithNumbersAndBlankLines) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_DECIMAL;
    settings.decimalDigits = 20;
    settings.outputQuantity = 3;
    settings.numberListItems = true;
    settings.blankLineBetweenItems = true;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    CHECK(!result.output.empty() && result.output.back() == '\0');
    const std::vector<std::string> lines = SplitAt(TextBeforeNull(result.output), '\n');
    CHECK_EQ(lines.size(), size_t(5));
    for (size_t item = 0; item < 3 && lines.size() == 5; item++) {
        const std::string& line = lines[item * 2];
        const std::string prefix = std::to_string(item + 1) + ". 0.";
        CHECK_EQ(line.substr(0, prefix.size()), prefix);
        CHECK(AllDigits(line.substr(prefix.size())));
        CHECK_EQ(line.size(), prefix.size() + 20);
    }
    if (lines.size() == 5) {
        CHECK(lines[1].empty());
        CHECK(lines[3].empty());
    }
    CHECK_EQ(result.binaryOutput.size(), size_t(60));
    CHECK(std::all_of(result.binaryOutput.begin(), result.binaryOutput.end(),
                      [](uint8_t digit) { return digit < 10; }));
}

TEST(IntegerOutputStaysInRangeWithSwappedBounds) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_INTEGER;
    settings.integerMin = 5;
    settings.integerMax = -5;
    settings.outputQuantity = 300;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    const std::vector<std::string> lines = SplitAt(TextBeforeNull(result.output), '\n');
    CHECK_EQ(lines.size(), size_t(300));
    std::set<long long> seen;
    size_t outOfRange = 0;
    for (const std::string& line : lines) {
        const long long value = std::stoll(line);
        if (value < -5 || value > 5)
            outOfRange++;
        seen.insert(value);
    }
    CHECK_EQ(outOfRange, size_t(0));
    CHECK_EQ(seen.size(), size_t(11));
    CHECK_EQ(result.binaryOutput.size(), 300 * INTEGER_BINARY_BYTES);
    if (!lines.empty() && result.binaryOutput.size() >= INTEGER_BINARY_BYTES)
        CHECK_EQ(LittleEndianInt64(result.binaryOutput, 0), static_cast<int64_t>(std::stoll(lines[0])));
}

TEST(IntegerRejectionWorksWithNegativeBounds) {
    std::vector<uint8_t> bytes;
    for (uint64_t draw : {uint64_t(1), uint64_t(2)}) {
        for (int shift = 56; shift >= 0; shift -= 8)
            bytes.push_back(static_cast<uint8_t>(draw >> shift));
    }
    std::vector<uint8_t> binaryValue;
    bool allDrawsRejected = true;
    CHECK_EQ(TextOf(CSPRNG::GenerateInteger(bytes, -3, 3, allDrawsRejected, binaryValue)),
             std::string("-1"));
    CHECK(!allDrawsRejected);
    CHECK_EQ(LittleEndianInt64(binaryValue, 0), int64_t(-1));
}

TEST(RandomCharactersUseOnlyTheTickedSets) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_RANDOM_CHARACTERS;
    settings.randomCharactersLength = 50;
    settings.outputQuantity = 4;
    settings.includeNumbers = false;
    settings.includeUppercase = true;
    settings.includeLowercase = false;
    settings.includeSpecial = false;

    const GenerationResult upper = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(upper.success);
    const std::vector<std::string> lines = SplitAt(TextBeforeNull(upper.output), '\n');
    CHECK_EQ(lines.size(), size_t(4));
    for (const std::string& line : lines) {
        CHECK_EQ(line.size(), size_t(50));
        CHECK(std::all_of(line.begin(), line.end(),
                          [](char character) { return character >= 'A' && character <= 'Z'; }));
    }

    settings.includeUppercase = false;
    settings.includeNumbers = true;
    settings.includeSpecial = true;
    settings.outputQuantity = 1;
    const std::string allowed = std::string("0123456789") + "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    const GenerationResult mixed = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(mixed.success);
    const std::string text = TextBeforeNull(mixed.output);
    CHECK_EQ(text.size(), size_t(50));
    CHECK(text.find_first_not_of(allowed) == std::string::npos);
}

TEST(PassphraseHasTheRequestedWordsAndNumbers) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_PASSPHRASE;
    settings.passphraseWordList = WORD_LIST_EFF_LARGE;
    settings.passphraseWordCount = 6;
    settings.passphraseNumberCount = 3;
    SetBuffer(settings.passphraseSeparator, " ");

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    const WordLists::WordList& wordList = WordLists::Get(WORD_LIST_EFF_LARGE);
    const std::set<std::string> words(wordList.words.begin(), wordList.words.end());
    const std::vector<std::string> tokens = SplitAt(TextBeforeNull(result.output), ' ');
    CHECK_EQ(tokens.size(), size_t(6));
    size_t digits = 0;
    size_t unknownWords = 0;
    for (std::string token : tokens) {
        if (!token.empty() && IsDigit(token.front())) {
            digits++;
            token.erase(token.begin());
        }
        if (!token.empty() && IsDigit(token.back())) {
            digits++;
            token.pop_back();
        }
        if (words.count(token) == 0)
            unknownWords++;
    }
    CHECK_EQ(digits, size_t(3));
    CHECK_EQ(unknownWords, size_t(0));
}

TEST(RawDataOutputMatchesItsLayoutInEveryEncoding) {
    FeedFullMicrophoneStartup();
    const int AMOUNT_BITS = 100;
    for (int encoding = 0; encoding < CSPRNG::RAW_ENCODING_COUNT; encoding++) {
        OutputSettings settings;
        settings.outputFormat = FORMAT_RAW_DATA;
        settings.rawDataEncoding = encoding;
        settings.rawDataAmount = AMOUNT_BITS;
        settings.rawDataUnit = RAW_DATA_UNIT_BITS;
        settings.rawDataGroupingEnabled.fill(false);

        const CSPRNG::RawDataLayout layout = CSPRNG::GetRawDataLayout(encoding, AMOUNT_BITS);
        const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
        CHECK(result.success);
        CHECK_EQ(TextBeforeNull(result.output).size(), static_cast<size_t>(layout.characterCount));
        const uint64_t expectedBinaryBytes =
            layout.usesRandomDigits ? layout.characterCount : layout.randomByteCount;
        CHECK_EQ(result.binaryOutput.size(), static_cast<size_t>(expectedBinaryBytes));
        CHECK(layout.bitsCarried >= AMOUNT_BITS);
    }
}

TEST(UuidOutputsAreWellFormedAndUnique) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_UUID_V4;
    settings.outputQuantity = 64;
    const GenerationResult v4 = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(v4.success);
    const std::vector<std::string> v4Lines = SplitAt(TextBeforeNull(v4.output), '\n');
    CHECK_EQ(v4Lines.size(), size_t(64));
    CHECK_EQ(std::set<std::string>(v4Lines.begin(), v4Lines.end()).size(), size_t(64));
    for (const std::string& uuid : v4Lines) {
        CHECK_EQ(uuid.size(), UUID_TEXT_LENGTH);
        if (uuid.size() == UUID_TEXT_LENGTH) {
            CHECK_EQ(uuid[14], '4');
            CHECK(std::string("89ab").find(uuid[19]) != std::string::npos);
        }
    }
    CHECK_EQ(v4.binaryOutput.size(), size_t(64 * 16));

    settings.outputFormat = FORMAT_UUID_V7;
    for (int ordering : {UUIDV7_DEDICATED_COUNTER, UUIDV7_MONOTONIC_RANDOM, UUIDV7_FULLY_RANDOM}) {
        settings.uuidV7Ordering = ordering;
        const GenerationResult v7 = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
        CHECK(v7.success);
        const std::vector<std::string> lines = SplitAt(TextBeforeNull(v7.output), '\n');
        CHECK_EQ(lines.size(), size_t(64));
        for (size_t i = 0; i < lines.size(); i++) {
            CHECK_EQ(lines[i].size(), UUID_TEXT_LENGTH);
            if (lines[i].size() == UUID_TEXT_LENGTH)
                CHECK_EQ(lines[i][14], '7');
            if (ordering != UUIDV7_FULLY_RANDOM && i > 0)
                CHECK(lines[i] > lines[i - 1]);
        }
    }
}

TEST(UuidV7CounterLengthsStayOrdered) {
    for (int counterBits : {AppConfig::UUIDV7_MIN_COUNTER_BITS, 12, 13, 20,
                            AppConfig::UUIDV7_MAX_COUNTER_BITS}) {
        CSPRNG::UUIDv7Options options;
        options.ordering = UUIDV7_DEDICATED_COUNTER;
        options.counterBits = counterBits;
        CSPRNG::UUIDv7State state;
        std::string previous;
        size_t outOfOrder = 0;
        for (uint64_t i = 0; i < 500; i++) {
            std::vector<uint8_t> binaryValue;
            const std::string uuid = TextOf(CSPRNG::GenerateUUIDv7(
                Tests::RandomBytes(900 + i, 16), false, false, options, state, binaryValue));
            if (uuid.size() != 32 || uuid[12] != '7' || uuid <= previous)
                outOfOrder++;
            previous = uuid;
        }
        CHECK_EQ(outOfOrder, size_t(0));
    }
    for (int incrementBits : {AppConfig::UUIDV7_MIN_INCREMENT_BITS, 32,
                              AppConfig::UUIDV7_MAX_INCREMENT_BITS}) {
        CSPRNG::UUIDv7Options options;
        options.ordering = UUIDV7_MONOTONIC_RANDOM;
        options.incrementBits = incrementBits;
        CSPRNG::UUIDv7State state;
        std::string previous;
        size_t outOfOrder = 0;
        for (uint64_t i = 0; i < 500; i++) {
            std::vector<uint8_t> binaryValue;
            const std::string uuid = TextOf(CSPRNG::GenerateUUIDv7(
                Tests::RandomBytes(1900 + i, 16), false, false, options, state, binaryValue));
            if (uuid.size() != 32 || uuid[12] != '7' || uuid <= previous)
                outOfOrder++;
            previous = uuid;
        }
        CHECK_EQ(outOfOrder, size_t(0));
    }
}

TEST(OneTimePadTextResultDecryptsToTheMessage) {
    FeedFullMicrophoneStartup();
    const std::string message = "Attack at dawn 42";
    OutputSettings settings;
    settings.outputFormat = FORMAT_ONE_TIME_PAD;
    settings.oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;
    SetBuffer(settings.oneTimePadMessage, message);

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    CHECK(result.mode == GenerationMode::TrueRandom);
    CHECK_EQ(result.oneTimePadResultMode, static_cast<int>(ONE_TIME_PAD_TEXT_INPUT));
    CHECK_EQ(result.oneTimePadKey.size(), message.size() + 1);
    CHECK_EQ(result.oneTimePadCiphertext.size(), message.size() + 1);
    std::string decrypted;
    for (size_t i = 0; i < message.size() && i + 1 < result.oneTimePadKey.size() &&
                       i + 1 < result.oneTimePadCiphertext.size();
         i++) {
        const int value = (OneTimePadIndex(result.oneTimePadCiphertext[i]) -
                           OneTimePadIndex(result.oneTimePadKey[i]) + OTP_ALPHABET_SIZE) %
                          OTP_ALPHABET_SIZE;
        decrypted.push_back(CSPRNG::ONE_TIME_PAD_ALPHABET[value]);
    }
    CHECK_EQ(decrypted, message);
}

TEST(OneTimePadRejectsPunctuationWithoutUsingEntropy) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_ONE_TIME_PAD;
    settings.oneTimePadInputMode = ONE_TIME_PAD_TEXT_INPUT;
    SetBuffer(settings.oneTimePadMessage, "Hi!");

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(!result.success);
    CHECK(Contains(result.errorMessage, "Message may only contain"));
    CHECK(result.oneTimePadKey.empty());
    CHECK(!Pool().HasUsedData());
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
}

TEST(TwoOutputsAreNeverTheSame) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_DECIMAL;
    settings.decimalDigits = 40;
    const GenerationResult first = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    const GenerationResult second = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(first.success && second.success);
    CHECK(TextBeforeNull(first.output) != TextBeforeNull(second.output));
}

TEST(TwoTrueRandomOutputsUseTheirOwnReserveBits) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_RAW_DATA;
    settings.rawDataEncoding = CSPRNG::RAW_HEX;
    settings.rawDataAmount = 256;
    settings.rawDataUnit = RAW_DATA_UNIT_BITS;

    const GenerationResult first = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(first.mode == GenerationMode::TrueRandom);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS - 256);
    const GenerationResult second = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(second.mode == GenerationMode::TrueRandom);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS - 512);
    CHECK(first.binaryOutput != second.binaryOutput);
}

TEST(HugeOutputFailsWithoutUsingEntropy) {
    FeedFullMicrophoneStartup();
    OutputSettings settings;
    settings.outputFormat = FORMAT_RAW_DATA;
    settings.rawDataEncoding = CSPRNG::RAW_HEX;
    settings.rawDataAmount = INT_MAX;
    settings.rawDataUnit = RAW_DATA_UNIT_BYTES;
    settings.outputQuantity = INT_MAX;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(!result.success);
    CHECK(!result.errorMessage.empty());
    CHECK(result.output.empty());
    CHECK(!Pool().HasUsedData());
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
}

TEST(ListPickOutputMatchesItsBinaryIndices) {
    FeedFullMicrophoneStartup();
    const std::vector<std::string> items = {"alpha", "beta", "gamma", "delta"};
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "alpha\nbeta\ngamma\ndelta");
    settings.outputQuantity = 4;
    settings.numberListItems = true;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    const std::vector<std::string> lines = SplitAt(TextBeforeNull(result.output), '\n');
    CHECK_EQ(lines.size(), size_t(4));
    CHECK_EQ(result.binaryOutput.size(), 4 * LIST_INDEX_BYTES);
    std::set<std::string> picked;
    size_t mismatches = 0;
    for (size_t i = 0; i < lines.size() && (i + 1) * LIST_INDEX_BYTES <= result.binaryOutput.size();
         i++) {
        const std::string prefix = std::to_string(i + 1) + ". ";
        const uint32_t index = LittleEndianUint32(result.binaryOutput, i * LIST_INDEX_BYTES);
        if (lines[i].compare(0, prefix.size(), prefix) != 0 || index >= items.size() ||
            lines[i].substr(prefix.size()) != items[index])
            mismatches++;
        else
            picked.insert(items[index]);
    }
    CHECK_EQ(mismatches, size_t(0));
    CHECK_EQ(picked.size(), size_t(4));
}

TEST(ListShuffleKeepsDuplicateItems) {
    FeedFullMicrophoneStartup();
    const GenerationResult result =
        CSPRNG::GenerateOutput(MICROPHONE_ONLY, ListSettings(FORMAT_LIST_SHUFFLE, "a\na\nb\nc"));
    CHECK(result.success);
    std::vector<std::string> lines = SplitAt(TextBeforeNull(result.output), '\n');
    std::sort(lines.begin(), lines.end());
    const std::vector<std::string> expected = {"a", "a", "b", "c"};
    CHECK(lines == expected);
}

TEST(ListPickUsesTheReserveForTrueRandom) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "a\nb\nc\nd\ne");
    settings.outputQuantity = 3;
    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    CHECK(result.mode == GenerationMode::TrueRandom);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY),
             FULL_STARTUP_RESERVE_BITS - CSPRNG::ListPickByteCount(3) * BITS_PER_BYTE);
}

TEST(InPlaceShuffleJoinsWithTheSeparatorAsTyped) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_SHUFFLE, "a | b|c\nd");
    settings.listOneItemPerLine = false;
    SetBuffer(settings.listSeparator, " | ");
    settings.shuffleListInPlace = true;

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    CHECK(result.updatesListText);
    CHECK(result.binaryOutput.empty());
    const std::string rewritten = TextOf(result.updatedListText);
    CHECK_EQ(rewritten.size(), size_t(4 + 3 * 3));
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        const size_t end = rewritten.find(" | ", start);
        parts.push_back(rewritten.substr(start, end == std::string::npos ? std::string::npos
                                                                         : end - start));
        if (end == std::string::npos)
            break;
        start = end + 3;
    }
    std::sort(parts.begin(), parts.end());
    const std::vector<std::string> expected = {"a", "b", "c", "d"};
    CHECK(parts == expected);
}

TEST(RemovePickedItemsSkipsZeroWeights) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_PICK, "keep\ngo1\ngo2");
    settings.outputQuantity = 2;
    settings.listRemovePicked = true;
    settings.listUseWeights = true;
    SetWeight(settings.listItemWeights, "keep", 0);

    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    CHECK(result.success);
    CHECK(result.updatesListText);
    CHECK_EQ(result.removedListItemCount, size_t(2));
    CHECK_EQ(TextOf(result.updatedListText), std::string("keep"));
    std::vector<std::string> lines = SplitAt(TextBeforeNull(result.output), '\n');
    std::sort(lines.begin(), lines.end());
    const std::vector<std::string> expected = {"go1", "go2"};
    CHECK(lines == expected);
}

TEST(CancelledListOutputsUseNothing) {
    FeedFullMicrophoneStartup();
    OutputSettings settings = ListSettings(FORMAT_LIST_SHUFFLE, "a\nb\nc");
    g_state.cancelGenerationRequested = true;
    const GenerationResult result = CSPRNG::GenerateOutput(MICROPHONE_ONLY, settings);
    g_state.cancelGenerationRequested = false;
    CHECK(result.cancelled);
    CHECK(!result.success);
    CHECK(!result.updatesListText);
    CHECK(result.output.empty());
    CHECK(!Pool().HasUsedData());
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
}

TEST(RequiredEntropyForEachFormat) {
    ResetOutputSettingsInAppState();

    g_state.outputFormat = FORMAT_DECIMAL;
    g_state.decimalDigits = 1000;
    CHECK(CloseTo(CalculateRequiredEntropy(), 1000 * std::log2(10.0)));
    g_state.decimalDigits = 16;
    CHECK(CloseTo(CalculateRequiredEntropy(), AppConfig::MINIMUM_ENTROPY_BITS));

    g_state.outputFormat = FORMAT_RANDOM_CHARACTERS;
    g_state.randomCharactersLength = 100;
    g_state.includeSpecial = true;
    g_state.outputQuantity = 2;
    CHECK(CloseTo(CalculateRequiredEntropy(), 200 * std::log2(94.0)));

    g_state.outputFormat = FORMAT_UUID_V4;
    g_state.outputQuantity = 10;
    CHECK(CloseTo(CalculateRequiredEntropy(), 1220.0));

    g_state.outputFormat = FORMAT_ONE_TIME_PAD;
    g_state.oneTimePadInputMode = ONE_TIME_PAD_MESSAGE_LENGTH_INPUT;
    g_state.oneTimePadMessageLength = 1000;
    CHECK(CloseTo(CalculateRequiredEntropy(), 1000 * std::log2(63.0)));

    SetBuffer(g_state.listText, NumberedListText(1000));
    g_state.outputFormat = FORMAT_LIST_PICK;
    g_state.outputQuantity = 100;
    g_state.listAllowRepeats = true;
    CHECK(CloseTo(CalculateRequiredEntropy(), 100 * std::log2(1000.0)));
    g_state.listAllowRepeats = false;
    CHECK(CloseTo(CalculateRequiredEntropy(), CSPRNG::ListPickBits(1000, 100, false)));

    SetBuffer(g_state.listText, NumberedListText(300));
    g_state.outputFormat = FORMAT_LIST_SHUFFLE;
    g_state.outputQuantity = 7;
    CHECK(CloseTo(CalculateRequiredEntropy(), CSPRNG::ListShuffleBits(300)));

    ResetOutputSettingsInAppState();
}

TEST(CapturedListSettingsNeverHoldTheItems) {
    ResetOutputSettingsInAppState();
    g_state.outputFormat = FORMAT_LIST_PICK;
    g_state.outputQuantity = 2;
    SetBuffer(g_state.listText, "secret one\nsecret two\nsecret three");

    const std::vector<OutputSetting> perLine = CaptureOutputSettings();
    size_t leaks = 0;
    for (const OutputSetting& setting : perLine) {
        if (Contains(setting.value, "secret"))
            leaks++;
    }
    CHECK_EQ(leaks, size_t(0));
    const OutputSetting* items = FindSetting(perLine, "items");
    CHECK(items && items->value == "3");
    const OutputSetting* quantity = FindSetting(perLine, "quantity");
    CHECK(quantity && quantity->value == "2");
    CHECK(FindSetting(perLine, "separator") == nullptr);
    CHECK(FindSetting(perLine, "allowRepeats") != nullptr);
    CHECK(FindSetting(perLine, "weights") != nullptr);

    g_state.listOneItemPerLine = false;
    const std::vector<OutputSetting> joined = CaptureOutputSettings();
    const OutputSetting* joinedSeparator = FindSetting(joined, "separator");
    CHECK(joinedSeparator && joinedSeparator->value == ", " && joinedSeparator->isText);

    g_state.outputFormat = FORMAT_LIST_SHUFFLE;
    const std::vector<OutputSetting> shuffle = CaptureOutputSettings();
    CHECK(FindSetting(shuffle, "quantity") == nullptr);
    CHECK(FindSetting(shuffle, "allowRepeats") == nullptr);

    ResetOutputSettingsInAppState();
}

TEST(CsvNumberedListItemsAreEscapedAfterTheNumber) {
    GeneratedOutputDetails details;
    details.format = FORMAT_LIST_PICK;
    details.type = "Pick From List";
    details.numberedItems = true;
    CHECK_EQ(Csv(details, "1. =x\n\n2. ok"),
             std::string("\"#\",\"Pick From List\"\r\n\"1\",\"'=x\"\r\n\"2\",\"ok\"\r\n"));

    details.numberedItems = false;
    CHECK_EQ(Csv(details, "a\n\nb"), std::string("\"Pick From List\"\r\n\"a\"\r\n\"b\"\r\n"));
    CHECK_EQ(Csv(details, ""), std::string("\"Pick From List\"\r\n"));
    CHECK_EQ(Csv(details, "1. a"), std::string("\"Pick From List\"\r\n\"1. a\"\r\n"));
}

TEST(JsonExportsJoinedItemsOneByOne) {
    GeneratedOutputDetails details;
    details.format = FORMAT_LIST_SHUFFLE;
    details.type = "Shuffle List";
    details.joinedItemLengths = {2, 1};
    details.joinedSeparatorLength = 2;
    CHECK(Contains(Json(details, "ab, c"), "  \"items\": [\n    \"ab\",\n    \"c\"\n  ]\n}\n"));

    details.joinedItemLengths = {2, 50};
    CHECK(Contains(Json(details, "ab, c"), "  \"items\": [\n    \"ab\"\n  ]\n}\n"));
}

TEST(JsonKeepsUnicodeAndShowsEmptySettings) {
    GeneratedOutputDetails details;
    details.format = FORMAT_LIST_PICK;
    details.type = "Pick From List";
    const std::string json = Json(details, "caf\xC3\xA9");
    CHECK(Contains(json, "    \"caf\xC3\xA9\"\n"));
    CHECK(Contains(json, "  \"settings\": {},\n"));
    CHECK(Contains(Json(details, ""), "  \"items\": [\n  ]\n}\n"));
}

TEST(OneTimePadExportTextAndBinary) {
    const std::string text = TextOf(CSPRNG::BuildOneTimePadExportText("AB", "CD", 2, false, ""));
    CHECK(Contains(text, "-----BEGIN KEY-----\nAB\n-----END KEY-----\n"));
    CHECK(Contains(text, "-----BEGIN CIPHERTEXT-----\nCD\n-----END CIPHERTEXT-----\n"));
    CHECK(Contains(text, "mod 63"));

    const char fileKey[] = {'\x00', '\xFF'};
    const char fileCiphertext[] = {'\x7F', '\x80'};
    const std::string fileText = TextOf(
        CSPRNG::BuildOneTimePadExportText(fileKey, fileCiphertext, 2, true, "f.bin"));
    CHECK(Contains(fileText, "File:     f.bin (2 bytes)\n"));
    CHECK(Contains(fileText, "-----BEGIN KEY-----\n00FF\n-----END KEY-----\n"));
    CHECK(Contains(fileText, "-----BEGIN CIPHERTEXT-----\n7F80\n-----END CIPHERTEXT-----\n"));

    CHECK(CSPRNG::BuildOneTimePadExportBinary("0A", "a ", 2, false) ==
          (std::vector<uint8_t>{0, 10, 36, 62}));
    CHECK(CSPRNG::BuildOneTimePadExportBinary(fileKey, fileCiphertext, 2, true) ==
          (std::vector<uint8_t>{0x00, 0xFF, 0x7F, 0x80}));
    CHECK(CSPRNG::OneTimePadValues("Zz 9", 4) == (std::vector<uint8_t>{35, 61, 62, 9}));
}
