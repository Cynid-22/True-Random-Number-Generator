#include "self_test.h"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <vector>
#include "hmac_drbg.h"
#include "sha512.h"
#include "toeplitz.h"

namespace Crypto {

struct HashTestVector {
    const char* name;
    const char* message;
    const char* expectedHash;
};

struct HmacTestVector {
    const char* name;
    const char* key;
    const char* message;
    const char* expectedMac;
};

struct DrbgTestVector {
    const char* name;
    const char* entropyInput;
    const char* nonce;
    const char* personalization;
    const char* entropyInputReseed;
    const char* additionalInputReseed;
    const char* additionalInputs[2];
    const char* returnedBits;
};

static const char TWO_BLOCK_MESSAGE[] =
    "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
    "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

static const char TWO_BLOCK_MESSAGE_HASH[] =
    "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
    "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909";

static const HashTestVector SHA512_TESTS[] = {
    {"SHA-512 one block",
     "abc",
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    {"SHA-512 two blocks", TWO_BLOCK_MESSAGE, TWO_BLOCK_MESSAGE_HASH},
};

static const HmacTestVector HMAC_TESTS[] = {
    {"HMAC-SHA-512 RFC 4231 case 1",
     "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
     "4869205468657265",
     "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
     "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854"},
    {"HMAC-SHA-512 RFC 4231 case 2",
     "4a656665",
     "7768617420646f2079612077616e7420666f72206e6f7468696e673f",
     "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
     "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737"},
    {"HMAC-SHA-512 RFC 4231 case 6",
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
     "aaaaaa",
     "54657374205573696e67204c6172676572205468616e20426c6f636b2d53697a"
     "65204b6579202d2048617368204b6579204669727374",
     "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
     "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598"},
};

static const DrbgTestVector HMAC_DRBG_TESTS[] = {
    {"HMAC_DRBG without reseed",
     "35049f389a33c0ecb1293238fd951f8ffd517dfde06041d32945b3e26914ba15",
     "f7328760be6168e6aa9fb54784989a11",
     "",
     nullptr,
     nullptr,
     {"",
      ""},
     "e76491b0260aacfded01ad39fbf1a66a88284caa5123368a2ad9330ee48335e3"
     "c9c9ba90e6cbc9429962d60c1a6661edcfaa31d972b8264b9d4562cf18494128"
     "a092c17a8da6f3113e8a7edfcd4427082bd390675e9662408144971717303d8d"
     "c352c9e8b95e7f35fa2ac9f549b292bc7c4bc7f01ee0a577859ef6e82d79ef23"
     "892d167c140d22aac32b64ccdfeee2730528a38763b24227f91ac3ffe47fb115"
     "38e435307e77481802b0f613f370ffb0dbeab774fe1efbb1a80d01154a9459e7"
     "3ad361108bbc86b0914f095136cbe634555ce0bb263618dc5c367291ce082551"
     "8987154fe9ecb052b3f0a256fcc30cc14572531c9628973639beda456f2bddf6"},
    {"HMAC_DRBG with reseed and additional input",
     "da740cbc36057a8e282ae717fe7dfbb245e9e5d49908a0119c5dbcf0a1f2d5ab",
     "46561ff612217ba3ff91baa06d4b5440",
     "fc227293523ecb5b1e28c87863626627d958acc558a672b148ce19e2abd2dde4",
     "1d61d4d8a41c3254b92104fd555adae0569d1835bb52657ec7fbba0fe03579c5",
     "b9ed8e35ad018a375b61189c8d365b00507cb1b4510d21cac212356b5bbaa8b2",
     {"b7998998eaf9e5d34e64ff7f03de765b31f407899d20535573e670c1b402c26a",
      "2089d49d63e0c4df58879d0cb1ba998e5b3d1a7786b785e7cf13ca5ea5e33cfd"},
     "5b70f3e4da95264233efbab155b828d4e231b67cc92757feca407cc9615a6608"
     "71cb07ad1a2e9a99412feda8ee34dc9c57fa08d3f8225b30d29887d20907d123"
     "30fffd14d1697ba0756d37491b0a8814106e46c8677d49d9157109c402ad0c24"
     "7a2f50cd5d99e538c850b906937a05dbb8888d984bc77f6ca00b0e3bc97b16d6"
     "d25814a54aa12143afddd8b2263690565d545f4137e593bb3ca88a37b0aadf79"
     "726b95c61906257e6dc47acd5b6b7e4b534243b13c16ad5a0a1163c0099fce43"
     "f428cd27c3e6463cf5e9a9621f4b3d0b3d4654316f4707675df39278d5783823"
     "049477dcce8c57fdbd576711c91301e9bd6bb0d3e72dc46d480ed8f61fd63811"},
};

static const uint64_t SHORT_TOEPLITZ_INPUT_VALUES[] = {
    0x0123456789ABCDEF, 0xFEDCBA9876543210, 0x0F1E2D3C4B5A6978};
static constexpr uint64_t FULL_TOEPLITZ_INPUT_STEP = 0x9E3779B97F4A7C15;
static constexpr size_t FULL_TOEPLITZ_INPUT_VALUE_COUNT =
    ToeplitzExtractor::MAX_INPUT_BYTES / sizeof(uint64_t);
static constexpr uint8_t FIRST_INPUT_BIT_ONLY = 0x80;

static const char TOEPLITZ_FIRST_BIT_OUTPUT_EQUALS_SEED_BITS_131071_TO_131582[] =
    "7a44cc244edaff911daaa573554ddeade05d5b57ae0f5bcc76337891cc07bda0"
    "969123b4d320e462bf1c6c7c61b7d0e95d40f352bca751c18ce788fd44347439";

static const char TOEPLITZ_SHORT_OUTPUT[] =
    "aedc3a2b3aa8173e27ebd84d1d6a395b26c5b964ccb62a56f8a64b53eebaee9c"
    "5b846df14382dc394141eb67bc55419e830cf283d7103cf70be2c3a2dbd88f83";

static const char TOEPLITZ_FULL_OUTPUT[] =
    "c71ac9eec87f7d82e341848fd779d9c4aae891b7ee8bc11f785e489fa7c1ebe7"
    "7e57488adafa4950d225da41d1bad0e5418ac69549f9ee955ce677a6302fb78b";

static uint8_t HexDigitValue(char digit) {
    if (digit >= '0' && digit <= '9')
        return static_cast<uint8_t>(digit - '0');
    return static_cast<uint8_t>(digit - 'a' + 10);
}

static std::vector<uint8_t> HexToBytes(const char* hex) {
    std::vector<uint8_t> bytes(strlen(hex) / 2);
    for (size_t i = 0; i < bytes.size(); i++) {
        bytes[i] = static_cast<uint8_t>((HexDigitValue(hex[2 * i]) << 4) |
                                        HexDigitValue(hex[2 * i + 1]));
    }
    return bytes;
}

static ByteSpan SpanOf(const std::vector<uint8_t>& bytes) {
    return {bytes.data(), bytes.size()};
}

static bool HashMatches(const SHA512::Hash& hash, const char* expectedHex) {
    const std::vector<uint8_t> expected = HexToBytes(expectedHex);
    return std::equal(hash.begin(), hash.end(), expected.begin(), expected.end());
}

static bool PassesHashTest(const HashTestVector& test) {
    const auto* message = reinterpret_cast<const uint8_t*>(test.message);
    return HashMatches(SHA512::Compute(message, strlen(test.message)), test.expectedHash);
}

static bool PassesIncrementalHashTest() {
    const auto* message = reinterpret_cast<const uint8_t*>(TWO_BLOCK_MESSAGE);
    SHA512::Hasher hasher;
    for (size_t i = 0; i < strlen(TWO_BLOCK_MESSAGE); i++) {
        hasher.Update(message + i, 1);
    }
    return HashMatches(hasher.Finish(), TWO_BLOCK_MESSAGE_HASH);
}

static bool PassesHmacTest(const HmacTestVector& test) {
    const std::vector<uint8_t> key = HexToBytes(test.key);
    const std::vector<uint8_t> message = HexToBytes(test.message);
    return HashMatches(SHA512::HMAC(key, message), test.expectedMac);
}

static bool PassesDrbgTest(const DrbgTestVector& test) {
    const std::vector<uint8_t> entropyInput = HexToBytes(test.entropyInput);
    const std::vector<uint8_t> nonce = HexToBytes(test.nonce);
    const std::vector<uint8_t> personalization = HexToBytes(test.personalization);
    HmacDrbg drbg(SpanOf(entropyInput), SpanOf(nonce), SpanOf(personalization));

    if (test.entropyInputReseed) {
        const std::vector<uint8_t> entropyInputReseed = HexToBytes(test.entropyInputReseed);
        const std::vector<uint8_t> additionalInputReseed =
            HexToBytes(test.additionalInputReseed);
        drbg.Reseed(SpanOf(entropyInputReseed), SpanOf(additionalInputReseed));
    }

    const std::vector<uint8_t> expected = HexToBytes(test.returnedBits);
    std::vector<uint8_t> output(expected.size());
    for (const char* additionalInputHex : test.additionalInputs) {
        const std::vector<uint8_t> additionalInput = HexToBytes(additionalInputHex);
        if (!drbg.Generate(output.data(), output.size(), SpanOf(additionalInput)))
            return false;
    }
    return output == expected;
}

static std::vector<uint8_t> BigEndianBytes(const std::vector<uint64_t>& values) {
    std::vector<uint8_t> bytes;
    bytes.reserve(values.size() * sizeof(uint64_t));
    for (uint64_t value : values) {
        for (size_t i = sizeof(uint64_t); i-- > 0;)
            bytes.push_back(static_cast<uint8_t>(value >> (i * 8)));
    }
    return bytes;
}

static std::vector<uint64_t> FullToeplitzInputValues() {
    std::vector<uint64_t> values(FULL_TOEPLITZ_INPUT_VALUE_COUNT);
    for (size_t i = 0; i < values.size(); i++)
        values[i] = FULL_TOEPLITZ_INPUT_STEP * (2 * i + 1);
    return values;
}

static bool ToeplitzOutputMatches(const std::vector<uint8_t>& input, const char* expectedHex) {
    uint8_t output[ToeplitzExtractor::OUTPUT_BYTES];
    if (!ToeplitzExtractor::Extract(input.data(), input.size(), output))
        return false;
    const std::vector<uint8_t> expected = HexToBytes(expectedHex);
    return std::equal(output, output + sizeof(output), expected.begin(), expected.end());
}

static const char* FindFailingToeplitzTest() {
    if (!ToeplitzOutputMatches({FIRST_INPUT_BIT_ONLY},
                               TOEPLITZ_FIRST_BIT_OUTPUT_EQUALS_SEED_BITS_131071_TO_131582))
        return "Toeplitz extractor first input bit";
    const std::vector<uint64_t> shortValues(std::begin(SHORT_TOEPLITZ_INPUT_VALUES),
                                            std::end(SHORT_TOEPLITZ_INPUT_VALUES));
    if (!ToeplitzOutputMatches(BigEndianBytes(shortValues), TOEPLITZ_SHORT_OUTPUT))
        return "Toeplitz extractor short input";
    if (!ToeplitzOutputMatches(BigEndianBytes(FullToeplitzInputValues()), TOEPLITZ_FULL_OUTPUT))
        return "Toeplitz extractor full input";
    return nullptr;
}

const char* FindFailingSelfTest() {
    for (const HashTestVector& test : SHA512_TESTS) {
        if (!PassesHashTest(test))
            return test.name;
    }
    if (!PassesIncrementalHashTest())
        return "SHA-512 incremental";
    for (const HmacTestVector& test : HMAC_TESTS) {
        if (!PassesHmacTest(test))
            return test.name;
    }
    for (const DrbgTestVector& test : HMAC_DRBG_TESTS) {
        if (!PassesDrbgTest(test))
            return test.name;
    }
    return FindFailingToeplitzTest();
}

}
