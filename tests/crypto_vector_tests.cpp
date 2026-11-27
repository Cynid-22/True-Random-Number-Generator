#include "test_framework.h"
#include "test_data.h"
#include "../src/crypto/hmac_drbg.h"
#include "../src/crypto/secure_mem.h"
#include "../src/crypto/sha512.h"
#include "../src/crypto/toeplitz.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <utility>
#include <vector>

using Crypto::ByteSpan;
using Crypto::HmacDrbg;
using Crypto::SHA512;
using Crypto::ToeplitzExtractor;

namespace {

constexpr size_t MILLION = 1000000;
constexpr double BIT_BALANCE_SIGMAS = 7.0;

struct TextHashVector {
    const char* message;
    const char* expectedHash;
};

const TextHashVector SHA512_TEXT_VECTORS[] = {
    {"abc",
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"},
    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
     "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445"},
    {"abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
     "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
     "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909"},
    {"The quick brown fox jumps over the lazy dog",
     "07e547d9586f6a73f73fbac0435ed76951218fb7d0c8d788a309d785436bbb64"
     "2e93a252a954f23912547d1e8a3b5ed6e1bfd7097821233fa0538f3db854fee6"},
};

const char SHA512_MILLION_A[] =
    "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
    "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b";

const char HMAC_RFC4231_CASE1[] =
    "87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cde"
    "daa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854";
const char HMAC_RFC4231_CASE2[] =
    "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea250554"
    "9758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737";
const char HMAC_RFC4231_CASE6[] =
    "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f352"
    "6b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598";

std::string HexOf(const SHA512::Hash& hash) {
    return Tests::BytesToHex(hash.data(), hash.size());
}

ByteSpan SpanOf(const std::vector<uint8_t>& bytes) {
    return {bytes.data(), bytes.size()};
}

HmacDrbg Drbg(uint8_t seedByte, uint8_t nonceByte = 0x11, const std::vector<uint8_t>& personalization = {}) {
    const std::vector<uint8_t> entropyInput(64, seedByte);
    const std::vector<uint8_t> nonce(16, nonceByte);
    return HmacDrbg(SpanOf(entropyInput), SpanOf(nonce), SpanOf(personalization));
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

std::vector<uint8_t> HashBytes(const std::vector<uint8_t>& message) {
    const SHA512::Hash hash = SHA512::Compute(message);
    return std::vector<uint8_t>(hash.begin(), hash.end());
}

size_t OneBits(const std::vector<uint8_t>& bytes) {
    size_t ones = 0;
    for (uint8_t byte : bytes) {
        for (int bit = 0; bit < 8; bit++)
            ones += (byte >> bit) & 1;
    }
    return ones;
}

}

TEST(Sha512PublishedVectors) {
    for (const TextHashVector& vector : SHA512_TEXT_VECTORS)
        CHECK_EQ(HexOf(SHA512::Compute(Tests::BytesOf(vector.message))),
                 std::string(vector.expectedHash));
    CHECK_EQ(HexOf(SHA512::Compute(std::vector<uint8_t>(MILLION, 'a'))),
             std::string(SHA512_MILLION_A));
}

TEST(Sha512EverySplitPointMatchesOneShot) {
    const std::vector<uint8_t> message = Tests::RandomBytes(17, 300);
    const SHA512::Hash expected = SHA512::Compute(message);
    size_t mismatches = 0;
    for (size_t split = 0; split <= message.size(); split++) {
        SHA512::Hasher hasher;
        hasher.Update(message.data(), split);
        hasher.Update(message.data() + split, message.size() - split);
        if (hasher.Finish() != expected)
            mismatches++;
    }
    CHECK_EQ(mismatches, size_t(0));
}

TEST(Sha512IsSensitiveToEveryInputBit) {
    const std::vector<uint8_t> message = Tests::RandomBytes(18, 64);
    const SHA512::Hash original = SHA512::Compute(message);
    std::set<std::string> hashes;
    for (size_t bit = 0; bit < message.size() * 8; bit++) {
        std::vector<uint8_t> flipped = message;
        flipped[bit / 8] ^= static_cast<uint8_t>(1u << (bit % 8));
        const SHA512::Hash hash = SHA512::Compute(flipped);
        CHECK(hash != original);
        hashes.insert(HexOf(hash));
    }
    CHECK_EQ(hashes.size(), message.size() * 8);
}

TEST(HmacSha512MoreRfc4231Cases) {
    CHECK_EQ(HexOf(SHA512::HMAC(std::vector<uint8_t>(20, 0x0b), Tests::BytesOf("Hi There"))),
             std::string(HMAC_RFC4231_CASE1));
    CHECK_EQ(HexOf(SHA512::HMAC(Tests::BytesOf("Jefe"),
                                Tests::BytesOf("what do ya want for nothing?"))),
             std::string(HMAC_RFC4231_CASE2));
    CHECK_EQ(HexOf(SHA512::HMAC(std::vector<uint8_t>(131, 0xaa),
                                Tests::BytesOf("Test Using Larger Than Block-Size Key - Hash "
                                               "Key First"))),
             std::string(HMAC_RFC4231_CASE6));
}

TEST(HmacHashesOnlyKeysLongerThanABlock) {
    const std::vector<uint8_t> message = Tests::BytesOf("message");
    const std::vector<uint8_t> longKey = Tests::RandomBytes(19, SHA512::BLOCK_SIZE + 1);
    CHECK(SHA512::HMAC(longKey, message) == SHA512::HMAC(HashBytes(longKey), message));

    const std::vector<uint8_t> blockKey = Tests::RandomBytes(20, SHA512::BLOCK_SIZE);
    CHECK(SHA512::HMAC(blockKey, message) != SHA512::HMAC(HashBytes(blockKey), message));

    std::vector<uint8_t> paddedKey = Tests::BytesOf("short key");
    const SHA512::Hash shortMac = SHA512::HMAC(paddedKey, message);
    paddedKey.resize(SHA512::BLOCK_SIZE, 0);
    CHECK(SHA512::HMAC(paddedKey, message) == shortMac);
}

TEST(HmacDrbgOutputPrefixDoesNotDependOnLength) {
    HmacDrbg shortRequest = Drbg(0x5A);
    HmacDrbg longRequest = Drbg(0x5A);
    const std::vector<uint8_t> shortOutput = Generate(shortRequest, 64);
    const std::vector<uint8_t> longOutput = Generate(longRequest, 200);
    CHECK(std::equal(shortOutput.begin(), shortOutput.end(), longOutput.begin()));
}

TEST(HmacDrbgSeedNonceAndPersonalizationAllMatter) {
    HmacDrbg base = Drbg(0x5A);
    HmacDrbg otherNonce = Drbg(0x5A, 0x12);
    HmacDrbg personalized = Drbg(0x5A, 0x11, Tests::BytesOf("TRNG HMAC_DRBG"));
    const std::vector<uint8_t> baseOutput = Generate(base, 64);
    CHECK(Generate(otherNonce, 64) != baseOutput);
    CHECK(Generate(personalized, 64) != baseOutput);
}

TEST(HmacDrbgReseedIsDeterministicAndChangesTheState) {
    const std::vector<uint8_t> reseedInput(64, 0x77);
    const std::vector<uint8_t> additionalInput = Tests::BytesOf("extra");
    HmacDrbg first = Drbg(0x5A);
    HmacDrbg second = Drbg(0x5A);
    HmacDrbg withAdditionalInput = Drbg(0x5A);
    HmacDrbg notReseeded = Drbg(0x5A);
    first.Reseed(SpanOf(reseedInput), {});
    second.Reseed(SpanOf(reseedInput), {});
    withAdditionalInput.Reseed(SpanOf(reseedInput), SpanOf(additionalInput));

    const std::vector<uint8_t> firstOutput = Generate(first, 64);
    CHECK(Generate(second, 64) == firstOutput);
    CHECK(Generate(withAdditionalInput, 64) != firstOutput);
    CHECK(Generate(notReseeded, 64) != firstOutput);
}

TEST(HmacDrbgAdditionalInputChangesLaterOutputs) {
    const std::vector<uint8_t> additionalInput = Tests::BytesOf("mixed in once");
    HmacDrbg mixed = Drbg(0x5A);
    HmacDrbg plain = Drbg(0x5A);
    Generate(mixed, 32, SpanOf(additionalInput));
    Generate(plain, 32);
    CHECK(Generate(mixed, 32) != Generate(plain, 32));
}

TEST(HmacDrbgOutputBitsAreBalanced) {
    HmacDrbg drbg = Drbg(0x3C);
    const size_t BYTES = 1 << 16;
    const std::vector<uint8_t> output = Generate(drbg, BYTES);
    const double bits = static_cast<double>(BYTES) * 8.0;
    const double ones = static_cast<double>(OneBits(output));
    CHECK(std::abs(ones - bits / 2.0) <= BIT_BALANCE_SIGMAS * std::sqrt(bits / 4.0));
}

TEST(ToeplitzSingleBitInputsGiveDistinctOutputs) {
    const size_t INPUT_BYTES = 128;
    const std::vector<uint8_t> zeros(ToeplitzExtractor::OUTPUT_BYTES, 0);
    std::set<std::vector<uint8_t>> outputs;
    size_t zeroOutputs = 0;
    for (size_t bit = 0; bit < INPUT_BYTES * 8; bit++) {
        std::vector<uint8_t> input(INPUT_BYTES, 0);
        input[bit / 8] = static_cast<uint8_t>(1u << (bit % 8));
        const std::vector<uint8_t> output = Extract(input);
        if (output == zeros)
            zeroOutputs++;
        outputs.insert(output);
    }
    CHECK_EQ(zeroOutputs, size_t(0));
    CHECK_EQ(outputs.size(), INPUT_BYTES * 8);
}

TEST(ToeplitzOutputBitsAreBalanced) {
    const size_t SAMPLES = 200;
    const size_t INPUT_BYTES = 80;
    size_t ones = 0;
    for (size_t sample = 0; sample < SAMPLES; sample++)
        ones += OneBits(Extract(Tests::RandomBytes(500 + sample, INPUT_BYTES)));
    const double bits = static_cast<double>(SAMPLES * ToeplitzExtractor::OUTPUT_BITS);
    CHECK(std::abs(static_cast<double>(ones) - bits / 2.0) <=
          BIT_BALANCE_SIGMAS * std::sqrt(bits / 4.0));
}

TEST(SecureVectorHelpers) {
    std::vector<uint8_t> bytes(100, 0xAB);
    const size_t capacityBefore = bytes.capacity();
    Crypto::SecureClearVector(bytes);
    CHECK(bytes.empty());
    CHECK_EQ(bytes.capacity(), capacityBefore);

    std::vector<uint8_t> grown = {1, 2, 3};
    Crypto::ReserveWithoutLeavingCopies(grown, 1000);
    CHECK(grown == (std::vector<uint8_t>{1, 2, 3}));
    CHECK(grown.capacity() >= 1003);
    const size_t capacityAfterGrowing = grown.capacity();
    const uint8_t* dataAfterGrowing = grown.data();
    Crypto::ReserveWithoutLeavingCopies(grown, 10);
    CHECK_EQ(grown.capacity(), capacityAfterGrowing);
    CHECK(grown.data() == dataAfterGrowing);

    Crypto::SecureBytes locked(1000, 7);
    CHECK_EQ(locked.size(), size_t(1000));
    CHECK(std::all_of(locked.begin(), locked.end(), [](uint8_t byte) { return byte == 7; }));
}

TEST(SecureBoxCopiesMovesAndResets) {
    Crypto::SecureBox<int> box;
    CHECK(!box);
    box.Emplace(42);
    CHECK(box && *box == 42);

    Crypto::SecureBox<int> copy = box;
    CHECK(copy && *copy == 42);
    *copy = 7;
    CHECK_EQ(*box, 42);

    Crypto::SecureBox<int> moved = std::move(copy);
    CHECK(!copy);
    CHECK(moved && *moved == 7);

    box = moved;
    CHECK(box && *box == 7);
    box.Reset();
    CHECK(!box);
    CHECK(moved && *moved == 7);
}
