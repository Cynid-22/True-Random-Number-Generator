#include "test_framework.h"
#include "test_data.h"
#include "../src/core/app_state.h"
#include "../src/logic/csprng.h"
#include "../src/logic/formats/one_time_pad_format.h"
#include "../src/logic/formats/raw_data_format.h"
#include "../src/logic/output_generator.h"
#include <algorithm>

using CSPRNG::GenerationMode;
using CSPRNG::GenerationResult;
using CSPRNG::TRUE_PATH_RESEED_BYTES;
using CSPRNG::TRUE_PATH_RESEED_INTERVAL_BYTES;
using CSPRNG::TruePathReserveBytes;
using Entropy::EntropySource;
using Tests::FIRST_SAMPLE_SEED;
using Tests::MICROPHONE_ONLY;
using Tests::SamplePoints;

namespace {

constexpr double GENERATOR_SEED_BITS = 512.0;
constexpr double FULL_STARTUP_RESERVE_BITS = 12 * 512.0;
constexpr double KEYSTROKE_STARTUP_RESERVE_BITS = 3 * 512.0;
constexpr double KEYSTROKE_UNFINISHED_CHUNK_BITS = 62 * Tests::KEYSTROKE_BITS_PER_POINT;
constexpr int RESERVE_COVERED_RAW_BITS = 256;
constexpr int RESERVE_EXCEEDING_RAW_BYTES = 1024;
constexpr int SHORT_PAD_LENGTH = 16;
constexpr int LONG_PAD_LENGTH = 1000;
constexpr size_t SHORT_PAD_BYTES_DRAWN = 64;
constexpr size_t SMALL_OUTPUT_BYTES = 100;
constexpr double BITS_PER_BYTE = 8.0;

Entropy::EntropyPool& Pool() {
    return g_state.entropyPool;
}

Entropy::GenerationInput InputWithGenerator(size_t reserveByteCount) {
    Entropy::GenerationInput input;
    const std::vector<uint8_t> entropyInput(64, 0x5A);
    const std::vector<uint8_t> nonce(16, 0x11);
    input.generator.Emplace(Crypto::ByteSpan{entropyInput.data(), entropyInput.size()},
                            Crypto::ByteSpan{nonce.data(), nonce.size()}, Crypto::ByteSpan{});
    input.reserveBytes.resize(reserveByteCount);
    for (size_t i = 0; i < reserveByteCount; i++)
        input.reserveBytes[i] = static_cast<uint8_t>(i * 31 + 7);
    return input;
}

OutputSettings RawDataSettings(int amount, int unit) {
    OutputSettings settings;
    settings.outputFormat = FORMAT_RAW_DATA;
    settings.rawDataAmount = amount;
    settings.rawDataUnit = unit;
    settings.rawDataEncoding = CSPRNG::RAW_HEX;
    settings.outputQuantity = 1;
    return settings;
}

OutputSettings PadSettings(int messageLength) {
    OutputSettings settings;
    settings.outputFormat = FORMAT_ONE_TIME_PAD;
    settings.oneTimePadInputMode = ONE_TIME_PAD_MESSAGE_LENGTH_INPUT;
    settings.oneTimePadMessageLength = messageLength;
    return settings;
}

void FeedFullMicrophoneStartup() {
    Pool().SecureWipe();
    Pool().AddDataPoints(SamplePoints(EntropySource::Microphone, FIRST_SAMPLE_SEED, 0,
                                      Entropy::STARTUP_SAMPLE_COUNT),
                         MICROPHONE_ONLY);
}

bool Contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

std::string TextBeforeNull(const std::vector<char>& text) {
    return std::string(text.begin(), std::find(text.begin(), text.end(), '\0'));
}

}

TEST(TruePathReserveCountsReseedBytes) {
    const size_t interval = static_cast<size_t>(TRUE_PATH_RESEED_INTERVAL_BYTES);
    CHECK_EQ(TruePathReserveBytes(100, 0), size_t(100));
    CHECK_EQ(TruePathReserveBytes(interval, 0), interval);
    CHECK_EQ(TruePathReserveBytes(interval + 1, 0), interval + 1 + TRUE_PATH_RESEED_BYTES);
    CHECK_EQ(TruePathReserveBytes(3 * interval, 0), 3 * interval + 2 * TRUE_PATH_RESEED_BYTES);
    CHECK_EQ(TruePathReserveBytes(10, interval), 10 + TRUE_PATH_RESEED_BYTES);
    CHECK_EQ(TruePathReserveBytes(10, interval - 5), 10 + TRUE_PATH_RESEED_BYTES);
    CHECK_EQ(TruePathReserveBytes(SIZE_MAX, 0), SIZE_MAX);
}

TEST(TrueRandomOutputIsReserveXorGenerator) {
    Entropy::GenerationInput trueInput = InputWithGenerator(SMALL_OUTPUT_BYTES);
    Entropy::GenerationInput pseudoInput = InputWithGenerator(SMALL_OUTPUT_BYTES);
    const Crypto::SecureBytes reserve = trueInput.reserveBytes;

    const std::vector<uint8_t> trueBytes = CSPRNG::GenerateRandomBytes(
        trueInput, SMALL_OUTPUT_BYTES, GenerationMode::TrueRandom);
    const std::vector<uint8_t> pseudoBytes = CSPRNG::GenerateRandomBytes(
        pseudoInput, SMALL_OUTPUT_BYTES, GenerationMode::PseudoRandom);

    size_t mismatches = 0;
    size_t reserveBytesLeftUnwiped = 0;
    for (size_t i = 0; i < SMALL_OUTPUT_BYTES; i++) {
        if (static_cast<uint8_t>(trueBytes[i] ^ pseudoBytes[i]) != reserve[i])
            mismatches++;
        if (trueInput.reserveBytes[i] != 0)
            reserveBytesLeftUnwiped++;
    }
    CHECK_EQ(mismatches, size_t(0));
    CHECK_EQ(reserveBytesLeftUnwiped, size_t(0));
    CHECK_EQ(trueInput.reserveBytesUsed, SMALL_OUTPUT_BYTES);
    CHECK_EQ(pseudoInput.reserveBytesUsed, size_t(0));
}

TEST(TrueRandomReseedsFromTheReserveEvery16KiB) {
    const size_t outputBytes = static_cast<size_t>(TRUE_PATH_RESEED_INTERVAL_BYTES) + 1;
    const size_t reserveNeeded = TruePathReserveBytes(outputBytes, 0);

    Entropy::GenerationInput input = InputWithGenerator(reserveNeeded);
    CSPRNG::GenerateRandomBytes(input, outputBytes, GenerationMode::TrueRandom);
    CHECK_EQ(input.reserveBytesUsed, reserveNeeded);
    CHECK_EQ(input.generatorBytesSinceFullReseed, uint64_t(1));

    Entropy::GenerationInput shortInput = InputWithGenerator(reserveNeeded - 1);
    bool ranOut = false;
    try {
        CSPRNG::GenerateRandomBytes(shortInput, outputBytes, GenerationMode::TrueRandom);
    } catch (const CSPRNG::ReserveRanOut&) {
        ranOut = true;
    }
    CHECK(ranOut);
}

TEST(RandomByteCountIncludesRejectedDraws) {
    OutputSettings decimal;
    decimal.outputFormat = FORMAT_DECIMAL;
    decimal.decimalDigits = 16;
    CHECK_EQ(CSPRNG::RandomByteCount(decimal), size_t(64));
    decimal.decimalDigits = 100;
    decimal.outputQuantity = 3;
    CHECK_EQ(CSPRNG::RandomByteCount(decimal), size_t(1200));

    OutputSettings integer;
    integer.outputFormat = FORMAT_INTEGER;
    CHECK_EQ(CSPRNG::RandomByteCount(integer), size_t(64));

    OutputSettings characters;
    characters.outputFormat = FORMAT_RANDOM_CHARACTERS;
    characters.randomCharactersLength = 100;
    CHECK_EQ(CSPRNG::RandomByteCount(characters), size_t(400));

    CHECK_EQ(CSPRNG::RandomByteCount(RawDataSettings(RESERVE_COVERED_RAW_BITS, RAW_DATA_UNIT_BITS)),
             size_t(32));

    OutputSettings uuid;
    uuid.outputFormat = FORMAT_UUID_V4;
    uuid.outputQuantity = 10;
    CHECK_EQ(CSPRNG::RandomByteCount(uuid), size_t(160));

    CHECK_EQ(CSPRNG::RandomByteCount(PadSettings(SHORT_PAD_LENGTH)), SHORT_PAD_BYTES_DRAWN);

    OutputSettings passphrase;
    passphrase.outputFormat = FORMAT_PASSPHRASE;
    passphrase.passphraseWordCount = 6;
    passphrase.passphraseNumberCount = 0;
    CHECK_EQ(CSPRNG::RandomByteCount(passphrase), size_t(86));
    passphrase.passphraseNumberCount = 2;
    CHECK_EQ(CSPRNG::RandomByteCount(passphrase), size_t(101));
}

TEST(GenerationNeedsTheGeneratorSeed) {
    Pool().SecureWipe();
    const GenerationResult result = CSPRNG::GenerateOutput(
        MICROPHONE_ONLY, RawDataSettings(RESERVE_COVERED_RAW_BITS, RAW_DATA_UNIT_BITS));
    CHECK(!result.success);
    CHECK(Contains(result.errorMessage, "Collect at least 512 bits"));
}

TEST(SelfTestFailureBlocksGeneration) {
    FeedFullMicrophoneStartup();
    g_state.selfTestFailed = true;
    const GenerationResult result = CSPRNG::GenerateOutput(
        MICROPHONE_ONLY, RawDataSettings(RESERVE_COVERED_RAW_BITS, RAW_DATA_UNIT_BITS));
    g_state.selfTestFailed = false;
    CHECK(!result.success);
    CHECK(Contains(result.errorMessage, "self-test"));
    CHECK(!Pool().HasUsedData());
}

TEST(ModeFollowsReserveCoverage) {
    FeedFullMicrophoneStartup();

    const GenerationResult covered = CSPRNG::GenerateOutput(
        MICROPHONE_ONLY, RawDataSettings(RESERVE_COVERED_RAW_BITS, RAW_DATA_UNIT_BITS));
    CHECK(covered.success);
    CHECK(covered.mode == GenerationMode::TrueRandom);
    CHECK_EQ(TextBeforeNull(covered.output).size(), size_t(64));
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY),
             FULL_STARTUP_RESERVE_BITS - RESERVE_COVERED_RAW_BITS);
    CHECK_EQ(Pool().GetUsedBits(), GENERATOR_SEED_BITS);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), 0.0);
    CHECK(Pool().HasUsedData());

    const double reserveAfterCovered = Pool().GetReserveBits(MICROPHONE_ONLY);
    const GenerationResult notCovered = CSPRNG::GenerateOutput(
        MICROPHONE_ONLY, RawDataSettings(RESERVE_EXCEEDING_RAW_BYTES, RAW_DATA_UNIT_BYTES));
    CHECK(notCovered.success);
    CHECK(notCovered.mode == GenerationMode::PseudoRandom);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), reserveAfterCovered);
}

TEST(OneTimePadNeedsTrueRandom) {
    FeedFullMicrophoneStartup();

    const GenerationResult shortPad =
        CSPRNG::GenerateOutput(MICROPHONE_ONLY, PadSettings(SHORT_PAD_LENGTH));
    CHECK(shortPad.success);
    CHECK(shortPad.mode == GenerationMode::TrueRandom);
    const std::string key = TextBeforeNull(shortPad.output);
    CHECK_EQ(key.size(), size_t(SHORT_PAD_LENGTH));
    for (char character : key)
        CHECK(CSPRNG::IsOneTimePadCharacter(static_cast<unsigned char>(character)));
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY),
             FULL_STARTUP_RESERVE_BITS - SHORT_PAD_BYTES_DRAWN * BITS_PER_BYTE);

    const double reserveBefore = Pool().GetReserveBits(MICROPHONE_ONLY);
    const GenerationResult longPad =
        CSPRNG::GenerateOutput(MICROPHONE_ONLY, PadSettings(LONG_PAD_LENGTH));
    CHECK(!longPad.success);
    CHECK(Contains(longPad.errorMessage, "requires TRUE RANDOM"));
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), reserveBefore);
}

TEST(CancelledGenerationUsesNothing) {
    FeedFullMicrophoneStartup();
    g_state.cancelGenerationRequested = true;
    const GenerationResult result = CSPRNG::GenerateOutput(
        MICROPHONE_ONLY, RawDataSettings(RESERVE_COVERED_RAW_BITS, RAW_DATA_UNIT_BITS));
    g_state.cancelGenerationRequested = false;

    CHECK(result.cancelled);
    CHECK(!result.success);
    CHECK_EQ(Pool().GetReserveBits(MICROPHONE_ONLY), FULL_STARTUP_RESERVE_BITS);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), GENERATOR_SEED_BITS);
    CHECK_EQ(Pool().GetUsedBits(), 0.0);
    CHECK(!Pool().HasUsedData());
}

TEST(ReserveSeedsTheGeneratorWhenTickedSourcesHoldTooLittle) {
    FeedFullMicrophoneStartup();
    Pool().AddDataPoints(SamplePoints(EntropySource::Keystroke, FIRST_SAMPLE_SEED, 0,
                                      Entropy::STARTUP_SAMPLE_COUNT,
                                      Tests::KEYSTROKE_VALUE_SHIFT),
                         Tests::MICROPHONE_AND_KEYSTROKE);
    CHECK_EQ(Pool().GetAvailableBits(Tests::KEYSTROKE_ONLY), 0.0);
    CHECK_EQ(Pool().GetReserveBits(Tests::KEYSTROKE_ONLY), KEYSTROKE_STARTUP_RESERVE_BITS);
    CHECK_EQ(Pool().GetUnfinishedChunkBits(Tests::KEYSTROKE_ONLY),
             KEYSTROKE_UNFINISHED_CHUNK_BITS);

    const GenerationResult result = CSPRNG::GenerateOutput(
        Tests::KEYSTROKE_ONLY, RawDataSettings(RESERVE_COVERED_RAW_BITS, RAW_DATA_UNIT_BITS));
    CHECK(result.success);
    CHECK(result.mode == GenerationMode::TrueRandom);
    CHECK_EQ(Pool().GetReserveBits(Tests::KEYSTROKE_ONLY),
             KEYSTROKE_STARTUP_RESERVE_BITS - GENERATOR_SEED_BITS - RESERVE_COVERED_RAW_BITS);
    CHECK_EQ(Pool().GetUsedBits(), GENERATOR_SEED_BITS);
    CHECK_EQ(Pool().GetAvailableBits(MICROPHONE_ONLY), GENERATOR_SEED_BITS);
}
