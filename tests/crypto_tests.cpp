#include "test_framework.h"
#include "test_data.h"
#include "../src/crypto/hmac_drbg.h"
#include "../src/crypto/self_test.h"
#include "../src/crypto/sha512.h"
#include "../src/crypto/toeplitz.h"
#include <algorithm>
#include <iterator>

using Crypto::ByteSpan;
using Crypto::HmacDrbg;
using Crypto::SHA512;
using Crypto::ToeplitzExtractor;

namespace {

struct RepeatedLetterHashVector {
    size_t letterCount;
    const char* expectedHash;
};

const RepeatedLetterHashVector SHA512_PADDING_BOUNDARY_VECTORS[] = {
    {0, "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
        "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"},
    {111, "fa9121c7b32b9e01733d034cfc78cbf67f926c7ed83e82200ef8681819692176"
          "0b4beff48404df811b953828274461673c68d04e297b0eb7b2b4d60fc6b566a2"},
    {112, "c01d080efd492776a1c43bd23dd99d0a2e626d481e16782e75d54c2503b5dc32"
          "bd05f0f1ba33e568b88fd2d970929b719ecbb152f58f130a407c8830604b70ca"},
    {127, "828613968b501dc00a97e08c73b118aa8876c26b8aac93df128502ab360f91ba"
          "b50a51e088769a5c1eff4782ace147dce3642554199876374291f5d921629502"},
    {128, "b73d1929aa615934e61a871596b3f3b33359f42b8175602e89f7e06e5f658a24"
          "3667807ed300314b95cacdd579f3e33abdfbe351909519a846d465c59582f321"},
    {239, "52c853cb8d907f3d4d6b889beb027985d7c273486d75f8baf26f80d24e90c74c"
          "6c3de3e22131582380a7d14d43f2941a31385439cd6ddc469f628015e50bf286"},
};

const char HMAC_RFC4231_CASE3[] =
    "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39"
    "bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb";
const char HMAC_RFC4231_CASE4[] =
    "b0ba465637458c6990e5a8c5f61d4af7e576d97ff94b872de76f8050361ee3db"
    "a91ca5c11aa25eb4d679275cc5788063a5f19741120c4f2de2adebeb10a298dd";
const char HMAC_RFC4231_CASE7[] =
    "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944"
    "b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58";
const char HMAC_QUICK_BROWN_FOX[] =
    "b42af09057bac1e2d41708e48a902e09b5ff7f12ab428a4fe86653c73dd248fb"
    "82f948a549f7b791a5b41915ee4d1ec3935357e4e2317250d0372afa2ebeeb3a";

std::string HexOf(const SHA512::Hash& hash) {
    return Tests::BytesToHex(hash.data(), hash.size());
}

ByteSpan SpanOf(const std::vector<uint8_t>& bytes) {
    return {bytes.data(), bytes.size()};
}

HmacDrbg DrbgWithSeed(uint8_t seedByte) {
    const std::vector<uint8_t> entropyInput(64, seedByte);
    const std::vector<uint8_t> nonce(16, 0x11);
    return HmacDrbg(SpanOf(entropyInput), SpanOf(nonce), {});
}

std::vector<uint8_t> Generate(HmacDrbg& drbg, size_t byteCount, ByteSpan additionalInput = {}) {
    std::vector<uint8_t> output(byteCount);
    CHECK(drbg.Generate(output.data(), output.size(), additionalInput));
    return output;
}

std::vector<uint8_t> Extract(const std::vector<uint8_t>& input) {
    std::vector<uint8_t> output(ToeplitzExtractor::OUTPUT_BYTES);
    CHECK(ToeplitzExtractor::Extract(input.data(), input.size(), output.data()));
    return output;
}

}

TEST(LaunchSelfTestsPass) {
    CHECK(Crypto::FindFailingSelfTest() == nullptr);
}

TEST(Sha512PaddingBoundaries) {
    for (const RepeatedLetterHashVector& vector : SHA512_PADDING_BOUNDARY_VECTORS) {
        const std::vector<uint8_t> message(vector.letterCount, 'a');
        CHECK_EQ(HexOf(SHA512::Compute(message)), std::string(vector.expectedHash));
    }
}

TEST(Sha512IncrementalMatchesOneShot) {
    const std::vector<uint8_t> message = Tests::RandomBytes(7, 1000);
    const size_t pieceSizes[] = {1, 127, 128, 129, 0, 300};
    SHA512::Hasher hasher;
    size_t offset = 0;
    for (size_t piece = 0; offset < message.size(); piece++) {
        const size_t length =
            std::min(pieceSizes[piece % std::size(pieceSizes)], message.size() - offset);
        hasher.Update(message.data() + offset, length);
        offset += length;
    }
    CHECK(hasher.Finish() == SHA512::Compute(message));
}

TEST(HmacSha512Rfc4231) {
    CHECK_EQ(HexOf(SHA512::HMAC(std::vector<uint8_t>(20, 0xaa), std::vector<uint8_t>(50, 0xdd))),
             std::string(HMAC_RFC4231_CASE3));

    std::vector<uint8_t> case4Key(25);
    for (size_t i = 0; i < case4Key.size(); i++)
        case4Key[i] = static_cast<uint8_t>(i + 1);
    CHECK_EQ(HexOf(SHA512::HMAC(case4Key, std::vector<uint8_t>(50, 0xcd))),
             std::string(HMAC_RFC4231_CASE4));

    const std::vector<uint8_t> case7Message = Tests::BytesOf(
        "This is a test using a larger than block-size key and a larger than "
        "block-size data. The key needs to be hashed before being used by the HMAC "
        "algorithm.");
    CHECK_EQ(HexOf(SHA512::HMAC(std::vector<uint8_t>(131, 0xaa), case7Message)),
             std::string(HMAC_RFC4231_CASE7));
}

TEST(HmacKeyMatchesOneShotHmac) {
    const std::vector<uint8_t> key = Tests::BytesOf("key");
    const std::vector<uint8_t> message =
        Tests::BytesOf("The quick brown fox jumps over the lazy dog");
    const SHA512::HMACKey hmacKey(key.data(), key.size());

    CHECK_EQ(HexOf(hmacKey.Mac(message.data(), message.size())),
             std::string(HMAC_QUICK_BROWN_FOX));
    CHECK(hmacKey.Mac(message.data(), message.size()) == SHA512::HMAC(key, message));

    SHA512::Hasher streamed = hmacKey.BeginMessage();
    streamed.Update(message.data(), 10);
    streamed.Update(message.data() + 10, message.size() - 10);
    CHECK_EQ(HexOf(hmacKey.FinishMessage(streamed)), std::string(HMAC_QUICK_BROWN_FOX));
}

TEST(HmacDrbgRefusesOversizedRequest) {
    HmacDrbg drbg = DrbgWithSeed(0x5A);
    std::vector<uint8_t> output(HmacDrbg::MAX_BYTES_PER_REQUEST + 1);
    CHECK(!drbg.Generate(output.data(), output.size()));
    CHECK(drbg.Generate(output.data(), HmacDrbg::MAX_BYTES_PER_REQUEST));
}

TEST(HmacDrbgIsDeterministicAndInputSensitive) {
    HmacDrbg first = DrbgWithSeed(0x5A);
    HmacDrbg second = DrbgWithSeed(0x5A);
    HmacDrbg otherSeed = DrbgWithSeed(0x5B);
    CHECK(Generate(first, 100) == Generate(second, 100));
    CHECK(Generate(first, 100) != Generate(otherSeed, 100));

    HmacDrbg withoutAdditionalInput = DrbgWithSeed(0x5A);
    HmacDrbg withAdditionalInput = DrbgWithSeed(0x5A);
    const std::vector<uint8_t> additionalInput = Tests::BytesOf("additional input");
    CHECK(Generate(withAdditionalInput, 100, SpanOf(additionalInput)) !=
          Generate(withoutAdditionalInput, 100));

    HmacDrbg copy = first;
    CHECK(Generate(copy, 100) == Generate(first, 100));

    const std::vector<uint8_t> reseedInput(64, 0x77);
    copy.Reseed(SpanOf(reseedInput), {});
    CHECK(Generate(copy, 100) != Generate(first, 100));
}

TEST(HmacDrbgStateMovesOnAfterEveryRequest) {
    HmacDrbg drbg = DrbgWithSeed(0x5A);
    const std::vector<uint8_t> firstRequest = Generate(drbg, 64);
    const std::vector<uint8_t> secondRequest = Generate(drbg, 64);
    CHECK(firstRequest != secondRequest);
}

TEST(ToeplitzZeroInputGivesZeroOutput) {
    const std::vector<uint8_t> zeros(ToeplitzExtractor::OUTPUT_BYTES, 0);
    CHECK(Extract(std::vector<uint8_t>(1024, 0)) == zeros);
    CHECK(Extract(std::vector<uint8_t>()) == zeros);
}

TEST(ToeplitzIsLinear) {
    const std::vector<uint8_t> first = Tests::RandomBytes(11, 4096);
    const std::vector<uint8_t> second = Tests::RandomBytes(12, 4096);
    std::vector<uint8_t> combinedInput(first.size());
    for (size_t i = 0; i < first.size(); i++)
        combinedInput[i] = first[i] ^ second[i];

    const std::vector<uint8_t> firstOutput = Extract(first);
    const std::vector<uint8_t> secondOutput = Extract(second);
    std::vector<uint8_t> combinedOutputs(firstOutput.size());
    for (size_t i = 0; i < firstOutput.size(); i++)
        combinedOutputs[i] = firstOutput[i] ^ secondOutput[i];
    CHECK(Extract(combinedInput) == combinedOutputs);
}

TEST(ToeplitzTrailingZeroBytesDoNotChangeOutput) {
    const std::vector<uint8_t> input = Tests::RandomBytes(13, 800);
    std::vector<uint8_t> padded = input;
    padded.resize(input.size() + 64, 0);
    CHECK(Extract(input) == Extract(padded));
}

TEST(ToeplitzInputSizeLimit) {
    std::vector<uint8_t> output(ToeplitzExtractor::OUTPUT_BYTES);
    const std::vector<uint8_t> largest(ToeplitzExtractor::MAX_INPUT_BYTES, 0x01);
    const std::vector<uint8_t> tooLarge(ToeplitzExtractor::MAX_INPUT_BYTES + 1, 0x01);
    CHECK(ToeplitzExtractor::Extract(largest.data(), largest.size(), output.data()));
    CHECK(!ToeplitzExtractor::Extract(tooLarge.data(), tooLarge.size(), output.data()));
}
