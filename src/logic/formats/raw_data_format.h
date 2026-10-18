#pragma once
#include <cstdint>
#include <vector>

namespace CSPRNG {

enum RawEncoding : int {
    RAW_HEX = 0,
    RAW_BASE64 = 1,
    RAW_BINARY = 2,
    RAW_OCTAL = 3,
    RAW_DECIMAL = 4,
    RAW_DUODECIMAL = 5,
    RAW_BASE32 = 6,
    RAW_CROCKFORD32 = 7,
    RAW_BASE36 = 8,
    RAW_BASE45 = 9,
    RAW_BASE58 = 10,
    RAW_BASE62 = 11,
    RAW_BASE64URL = 12,
    RAW_BASE85 = 13,
    RAW_ENCODING_COUNT = 14
};

struct RawDataLayout {
    uint64_t characterCount;
    uint64_t randomByteCount;
    double bitsCarried;
    bool usesRandomDigits;
};

uint64_t RawDataBitCount(int amount, int unit);

RawDataLayout GetRawDataLayout(int encoding, uint64_t bits);

bool RawEncodingHasSingleCaseLetters(int encoding);

std::vector<char> GenerateRawData(
    const std::vector<uint8_t> &randomBytes,
    int amount,
    int unit,
    int encoding,
    int groupSize,
    bool uppercase);

}
