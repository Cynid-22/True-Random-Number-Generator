#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "../src/entropy/entropy_common.h"

namespace Tests {

inline uint8_t HexDigitValue(char digit) {
    if (digit >= '0' && digit <= '9')
        return static_cast<uint8_t>(digit - '0');
    if (digit >= 'A' && digit <= 'F')
        return static_cast<uint8_t>(digit - 'A' + 10);
    return static_cast<uint8_t>(digit - 'a' + 10);
}

inline std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes(hex.size() / 2);
    for (size_t i = 0; i < bytes.size(); i++)
        bytes[i] = static_cast<uint8_t>((HexDigitValue(hex[2 * i]) << 4) |
                                        HexDigitValue(hex[2 * i + 1]));
    return bytes;
}

inline std::string BytesToHex(const uint8_t* bytes, size_t length) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    std::string hex;
    for (size_t i = 0; i < length; i++) {
        hex.push_back(HEX_DIGITS[bytes[i] >> 4]);
        hex.push_back(HEX_DIGITS[bytes[i] & 0x0F]);
    }
    return hex;
}

inline std::vector<uint8_t> BytesOf(const char* text) {
    return std::vector<uint8_t>(text, text + strlen(text));
}

inline std::string TextOf(const std::vector<char>& text) {
    return std::string(text.begin(), text.end());
}

class SplitMix64 {
public:
    explicit SplitMix64(uint64_t seed) : m_state(seed) {}

    uint64_t Next() {
        m_state += 0x9E3779B97F4A7C15ULL;
        uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

private:
    uint64_t m_state;
};

inline std::vector<uint8_t> RandomBytes(uint64_t seed, size_t count) {
    SplitMix64 random(seed);
    std::vector<uint8_t> bytes(count);
    for (uint8_t& byte : bytes)
        byte = static_cast<uint8_t>(random.Next() >> 56);
    return bytes;
}

inline const std::set<Entropy::EntropySource> MICROPHONE_ONLY{
    Entropy::EntropySource::Microphone};
inline const std::set<Entropy::EntropySource> KEYSTROKE_ONLY{
    Entropy::EntropySource::Keystroke};
inline const std::set<Entropy::EntropySource> MICROPHONE_AND_KEYSTROKE{
    Entropy::EntropySource::Microphone, Entropy::EntropySource::Keystroke};

constexpr double MICROPHONE_BITS_PER_POINT = 8.0;
constexpr double KEYSTROKE_BITS_PER_POINT = 2.0;
constexpr int KEYSTROKE_VALUE_SHIFT = 8;
constexpr uint64_t FIRST_SAMPLE_SEED = 1;
constexpr uint64_t SECOND_SAMPLE_SEED = 2;

// Every value of these seeds earns the source's full credit cap (checked in Python)
inline Entropy::EntropyDataVector SamplePoints(Entropy::EntropySource source, uint64_t seed,
                                               size_t firstIndex, size_t count,
                                               int valueShift = 0) {
    static uint64_t s_timestamp = 0;
    SplitMix64 random(seed);
    for (size_t i = 0; i < firstIndex; i++)
        random.Next();
    Entropy::EntropyDataVector points;
    points.reserve(count);
    for (size_t i = 0; i < count; i++)
        points.push_back({++s_timestamp, random.Next() >> valueShift, source});
    return points;
}

}
