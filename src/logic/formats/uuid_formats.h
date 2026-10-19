#pragma once
#include <cstdint>
#include <vector>

namespace CSPRNG {

std::vector<char> GenerateUUIDv4(
    const std::vector<uint8_t> &randomBytes,
    bool uppercase,
    bool hyphens,
    std::vector<uint8_t> &binaryValue);

struct UUIDv7Options {
    int ordering = 0;
    int counterBits = 12;
    int incrementBits = 32;
};

struct UUIDv7State {
    uint64_t lastTimestampMs = 0;
    bool hasLastTimestamp = false;
    uint64_t counter = 0;
    uint64_t monotonicRandA = 0;
    uint64_t monotonicRandB = 0;
};

std::vector<char> GenerateUUIDv7(
    const std::vector<uint8_t> &randomBytes,
    bool uppercase,
    bool hyphens,
    const UUIDv7Options &options,
    UUIDv7State &state,
    std::vector<uint8_t> &binaryValue);

}
