#include "test_framework.h"
#include "test_data.h"
#include "../src/core/output_format.h"
#include "../src/logic/formats/number_formats.h"
#include "../src/logic/formats/one_time_pad_format.h"
#include "../src/logic/formats/raw_data_format.h"
#include "../src/logic/formats/text_formats.h"
#include "../src/logic/formats/uuid_formats.h"
#include "../src/logic/word_lists.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstring>

using Tests::BytesOf;
using Tests::TextOf;

namespace {

struct EncodingVector {
    int encoding;
    const char* input;
    const char* expected;
};

const EncodingVector WHOLE_BYTE_ENCODING_VECTORS[] = {
    {CSPRNG::RAW_BASE64, "foobar", "Zm9vYmFy"},
    {CSPRNG::RAW_BASE64, "fooba", "Zm9vYmE="},
    {CSPRNG::RAW_BASE64, "f", "Zg=="},
    {CSPRNG::RAW_BASE64URL, "fooba", "Zm9vYmE"},
    {CSPRNG::RAW_BASE32, "foobar", "MZXW6YTBOI======"},
    {CSPRNG::RAW_BASE32, "fo", "MZXQ===="},
    {CSPRNG::RAW_BASE45, "AB", "BB8"},
    {CSPRNG::RAW_BASE45, "Hello!!", "%69 VD92EX0"},
    {CSPRNG::RAW_BASE45, "base-45", "UJCLQE7W581"},
    {CSPRNG::RAW_BASE85, "foobar", "W^Zp|VR8"},
    {CSPRNG::RAW_BASE85, "hello world!", "Xk~0{Zy<MXa%^NF"},
    {CSPRNG::RAW_OCTAL, "foobar", "3146755730460562"},
};

constexpr bool UPPERCASE = true;
constexpr bool LOWERCASE = false;
constexpr int NO_GROUPING = 0;

std::string EncodeBytes(int encoding, const std::vector<uint8_t>& bytes, int amountInBytes,
                        bool uppercase = UPPERCASE, int groupSize = NO_GROUPING) {
    std::vector<uint8_t> binaryValue;
    return TextOf(CSPRNG::GenerateRawData(bytes, amountInBytes, RAW_DATA_UNIT_BYTES, encoding,
                                          groupSize, uppercase, binaryValue));
}

int OneTimePadIndex(char character) {
    const char* found = strchr(CSPRNG::ONE_TIME_PAD_ALPHABET, character);
    return found && character != '\0' ? static_cast<int>(found - CSPRNG::ONE_TIME_PAD_ALPHABET)
                                      : -1;
}

uint64_t UuidTimestampMs(const std::string& uuid) {
    const std::string timestampHex = uuid.substr(0, 8) + uuid.substr(9, 4);
    return std::stoull(timestampHex, nullptr, 16);
}

uint64_t NowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count());
}

}

TEST(DecimalRejectsDrawsAboveTheLastWholeRange) {
    std::vector<uint8_t> binaryValue;
    const std::vector<uint8_t> bytes = {0xFF, 0xFF, 0xFF, 0xFA, 0x00, 0x07};
    CHECK_EQ(TextOf(CSPRNG::GenerateDecimal(bytes, 1, binaryValue)), std::string("0.7"));
    CHECK(binaryValue == std::vector<uint8_t>{7});
    CHECK_THROWS(CSPRNG::GenerateDecimal(std::vector<uint8_t>(64, 0xFF), 1, binaryValue));
}

TEST(IntegerRejectsDrawsBelowTheBias) {
    std::vector<uint8_t> binaryValue;
    bool allDrawsRejected = true;
    const std::vector<uint8_t> bytes = {0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 13};
    CHECK_EQ(TextOf(CSPRNG::GenerateInteger(bytes, 0, 9, allDrawsRejected, binaryValue)),
             std::string("3"));
    CHECK(!allDrawsRejected);
    CHECK(binaryValue == (std::vector<uint8_t>{3, 0, 0, 0, 0, 0, 0, 0}));
    CHECK_EQ(TextOf(CSPRNG::GenerateInteger(bytes, 9, 0, allDrawsRejected, binaryValue)),
             std::string("3"));

    const std::vector<uint8_t> zeroDraw(8, 0);
    CHECK_EQ(TextOf(CSPRNG::GenerateInteger(zeroDraw, LLONG_MIN, LLONG_MAX, allDrawsRejected,
                                            binaryValue)),
             std::string("-9223372036854775808"));

    CHECK(CSPRNG::GenerateInteger(std::vector<uint8_t>(7, 0), 0, 9, allDrawsRejected,
                                  binaryValue)
              .empty());
    CHECK(allDrawsRejected);
}

TEST(RandomCharactersRejectBiasedDraws) {
    std::vector<uint8_t> binaryValue;
    const std::vector<uint8_t> bytes = {0xFF, 0xFA, 0x00, 0x03};
    CHECK_EQ(TextOf(CSPRNG::GenerateRandomCharacters(bytes, 1, true, false, false, false,
                                                     binaryValue)),
             std::string("3"));
    CHECK(binaryValue == std::vector<uint8_t>{3});
    CHECK_THROWS(CSPRNG::GenerateRandomCharacters(std::vector<uint8_t>(64, 0xFF), 1, true,
                                                  false, false, false, binaryValue));
    CHECK(CSPRNG::GenerateRandomCharacters(bytes, 1, false, false, false, false, binaryValue)
              .empty());
}

TEST(PassphraseRejectsBiasedDraws) {
    const std::vector<std::string> words = {"alpha", "bravo", "charlie"};
    const std::vector<uint8_t> bytes = {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x02, 0x00, 0x00, 0x04};
    std::vector<uint8_t> binaryValue;
    CHECK_EQ(TextOf(CSPRNG::GeneratePassphrase(bytes, 2, 0, "-", words, binaryValue)),
             std::string("charlie-bravo"));
    CHECK(binaryValue == (std::vector<uint8_t>{2, 0, 0, 0, 1, 0, 0, 0}));
    CHECK_THROWS(CSPRNG::GeneratePassphrase(bytes, 2, 5, "-", words, binaryValue));
    CHECK(std::fabs(CSPRNG::PassphraseNumberBits(1, 1) - (std::log2(10.0) + 1.0)) < 1e-9);
}

TEST(PassphraseNumbersGoIntoFreeSlots) {
    const std::vector<std::string> words = {"alpha", "bravo", "charlie"};
    const std::vector<uint8_t> bytes = Tests::RandomBytes(21, 256);
    std::vector<uint8_t> binaryValue;
    const std::string passphrase =
        TextOf(CSPRNG::GeneratePassphrase(bytes, 3, 4, " ", words, binaryValue));
    const auto digits = std::count_if(passphrase.begin(), passphrase.end(),
                                      [](char character) { return character >= '0' && character <= '9'; });
    CHECK_EQ(digits, 4);
}

TEST(WordListsLoadFromTheExe) {
    const size_t expectedWordCounts[WORD_LIST_COUNT] = {123565, 7776, 8192, 2048};
    for (int wordListId = 0; wordListId < WORD_LIST_COUNT; wordListId++) {
        const WordLists::WordList& list = WordLists::Get(wordListId);
        CHECK(list.IsUsable());
        CHECK_EQ(list.words.size(), expectedWordCounts[wordListId]);
        CHECK(std::fabs(list.bitsPerWord -
                        std::log2(static_cast<double>(expectedWordCounts[wordListId]))) < 1e-9);
    }
    CHECK(WordLists::Get(WORD_LIST_EFF_LARGE).noWordStartsAnother);
}

TEST(RawDataWholeByteEncodings) {
    for (const EncodingVector& vector : WHOLE_BYTE_ENCODING_VECTORS) {
        const std::vector<uint8_t> input = BytesOf(vector.input);
        CHECK_EQ(EncodeBytes(vector.encoding, input, static_cast<int>(input.size())),
                 std::string(vector.expected));
    }
}

TEST(RawDataHexCaseGroupingAndBinaryValue) {
    const std::vector<uint8_t> input = BytesOf("foobar");
    CHECK_EQ(EncodeBytes(CSPRNG::RAW_HEX, input, 6, LOWERCASE), std::string("666f6f626172"));
    CHECK_EQ(EncodeBytes(CSPRNG::RAW_HEX, input, 6, UPPERCASE, 4),
             std::string("666F 6F62 6172"));

    std::vector<uint8_t> binaryValue;
    CSPRNG::GenerateRawData(input, 6, RAW_DATA_UNIT_BYTES, CSPRNG::RAW_HEX, NO_GROUPING,
                            UPPERCASE, binaryValue);
    CHECK(binaryValue == input);
    CHECK_THROWS(CSPRNG::GenerateRawData(input, 7, RAW_DATA_UNIT_BYTES, CSPRNG::RAW_HEX,
                                         NO_GROUPING, UPPERCASE, binaryValue));
}

TEST(RawDataBitStreamEncodings) {
    std::vector<uint8_t> binaryValue;
    CHECK_EQ(TextOf(CSPRNG::GenerateRawData(BytesOf("f"), 8, RAW_DATA_UNIT_BITS,
                                            CSPRNG::RAW_BINARY, NO_GROUPING, UPPERCASE,
                                            binaryValue)),
             std::string("01100110"));

    std::vector<uint8_t> crockfordInput = BytesOf("foobar");
    crockfordInput.push_back(0x00);
    CHECK_EQ(EncodeBytes(CSPRNG::RAW_CROCKFORD32, crockfordInput, 6),
             std::string("CSQPYRK1E8"));
}

TEST(RawDataDigitsRejectBiasedBytes) {
    const CSPRNG::RawDataLayout layout = CSPRNG::GetRawDataLayout(CSPRNG::RAW_DECIMAL, 3);
    CHECK_EQ(layout.characterCount, uint64_t(1));
    std::vector<uint8_t> bytes(static_cast<size_t>(layout.randomByteCount), 0);
    bytes[0] = 250;
    bytes[1] = 255;
    bytes[2] = 7;
    std::vector<uint8_t> binaryValue;
    CHECK_EQ(TextOf(CSPRNG::GenerateRawData(bytes, 3, RAW_DATA_UNIT_BITS, CSPRNG::RAW_DECIMAL,
                                            NO_GROUPING, UPPERCASE, binaryValue)),
             std::string("7"));
    CHECK(binaryValue == std::vector<uint8_t>{7});
}

TEST(UuidV4SetsVersionAndVariant) {
    std::vector<uint8_t> binaryValue;
    CHECK_EQ(TextOf(CSPRNG::GenerateUUIDv4(std::vector<uint8_t>(16, 0x00), false, true,
                                           binaryValue)),
             std::string("00000000-0000-4000-8000-000000000000"));
    CHECK_EQ(TextOf(CSPRNG::GenerateUUIDv4(std::vector<uint8_t>(16, 0xFF), true, false,
                                           binaryValue)),
             std::string("FFFFFFFFFFFF4FFFBFFFFFFFFFFFFFFF"));
    CHECK(CSPRNG::GenerateUUIDv4(std::vector<uint8_t>(15, 0x00), false, true, binaryValue)
              .empty());
}

TEST(UuidV7IsTimeOrderedAndIncreasing) {
    const int orderings[] = {UUIDV7_DEDICATED_COUNTER, UUIDV7_MONOTONIC_RANDOM};
    for (int ordering : orderings) {
        CSPRNG::UUIDv7Options options;
        options.ordering = ordering;
        CSPRNG::UUIDv7State state;
        std::vector<uint8_t> binaryValue;
        std::string previous;
        for (uint64_t i = 0; i < 200; i++) {
            const std::string uuid = TextOf(CSPRNG::GenerateUUIDv7(
                Tests::RandomBytes(100 + i, 16), false, true, options, state, binaryValue));
            CHECK_EQ(uuid.size(), size_t(36));
            CHECK_EQ(uuid[14], '7');
            CHECK(uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b');
            CHECK(uuid > previous);
            const uint64_t timestampMs = UuidTimestampMs(uuid);
            CHECK(timestampMs + 10000 > NowMs() && timestampMs < NowMs() + 10000);
            previous = uuid;
        }
    }
}

TEST(OneTimePadKeyRejectsBiasedBytes) {
    std::vector<char> key;
    CHECK(CSPRNG::GenerateOneTimePadKey({252, 0, 64}, 2, key));
    CHECK_EQ(TextOf(key), std::string("01"));
    CHECK(!CSPRNG::GenerateOneTimePadKey({252, 253, 254, 255}, 1, key));
    CHECK(key.empty());
}

TEST(OneTimePadTextDecryptsWithItsKey) {
    const char message[] = "Hello World 2026";
    const size_t length = strlen(message);
    std::vector<char> key;
    std::vector<char> ciphertext;
    std::string error;
    CHECK(CSPRNG::GenerateOneTimePadText(Tests::RandomBytes(31, 4 * length), message, length,
                                         key, ciphertext, error));
    CHECK_EQ(key.size(), length + 1);
    CHECK_EQ(ciphertext.size(), length + 1);

    size_t wrongCharacters = 0;
    for (size_t i = 0; i < length && key.size() > length && ciphertext.size() > length; i++) {
        const int decrypted =
            (OneTimePadIndex(ciphertext[i]) - OneTimePadIndex(key[i]) +
             CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE) %
            CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE;
        if (CSPRNG::ONE_TIME_PAD_ALPHABET[decrypted] != message[i])
            wrongCharacters++;
    }
    CHECK_EQ(wrongCharacters, size_t(0));

    CHECK(!CSPRNG::GenerateOneTimePadText(Tests::RandomBytes(31, 64), "Hi!", 3, key, ciphertext,
                                          error));
    CHECK(!error.empty());
}

TEST(OneTimePadFileIsXorWithTheKey) {
    const std::vector<uint8_t> fileData = {0x01, 0x02, 0x03};
    std::vector<char> key;
    std::vector<char> ciphertext;
    CHECK(CSPRNG::GenerateOneTimePadFile({0xFF, 0x0F, 0x00, 0x99}, fileData, key, ciphertext));
    CHECK(key == (std::vector<char>{'\xFF', '\x0F', '\x00'}));
    CHECK(ciphertext == (std::vector<char>{'\xFE', '\x0D', '\x03'}));
    CHECK(!CSPRNG::GenerateOneTimePadFile({0xFF, 0x0F}, fileData, key, ciphertext));
}
