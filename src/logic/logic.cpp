#include "logic.h"
#include "formats/one_time_pad_format.h"
#include "formats/raw_data_format.h"
#include "../logging/logger.h"
#include "../core/app_state.h"
#include "../entropy/pool.h"
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <windows.h>
#include <sstream>
#include "../resource.h"

static const float PASSPHRASE_BITS_PER_WORD = 16.5f;

static void ReadWordList(std::istream& input) {
    g_state.cachedWordList.clear();
    g_state.cachedWordList.reserve(125000);

    std::string line;
    while (std::getline(input, line)) {
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            g_state.cachedWordList.push_back(line.substr(start, end - start + 1));
        }
    }
}

bool LoadWordListForGeneration() {
    try {
        if (!g_state.cachedWordList.empty()) {
            return true;
        }

        HRSRC resourceInfo = FindResourceA(NULL, MAKEINTRESOURCEA(IDR_WORDLIST), (LPCSTR)RT_RCDATA);
        if (resourceInfo) {
            HGLOBAL resourceHandle = LoadResource(NULL, resourceInfo);
            if (resourceHandle) {
                DWORD resourceSize = SizeofResource(NULL, resourceInfo);
                const char* resourceData = (const char*)LockResource(resourceHandle);
                if (resourceData && resourceSize > 0) {
                    std::string wordListText(resourceData, resourceSize);
                    std::istringstream wordListStream(wordListText);
                    ReadWordList(wordListStream);

                    Logger::Log(Logger::Level::INFO, "Logic", "Loaded %zu words from embedded resource", g_state.cachedWordList.size());
                    return !g_state.cachedWordList.empty();
                }
            }
        }

        std::vector<std::string> candidatePaths = {
            "assets/default_wordlist.txt",
            "./assets/default_wordlist.txt",
            "../assets/default_wordlist.txt",
            "default_wordlist.txt"
        };

        char executablePath[MAX_PATH];
        if (GetModuleFileNameA(NULL, executablePath, MAX_PATH) > 0) {
            std::string executableDirectory = executablePath;
            size_t lastSlash = executableDirectory.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                executableDirectory = executableDirectory.substr(0, lastSlash + 1);
                candidatePaths.insert(candidatePaths.begin(), executableDirectory + "assets/default_wordlist.txt");
                candidatePaths.insert(candidatePaths.begin(), executableDirectory + "assets\\default_wordlist.txt");
            }
        }

        std::ifstream file;
        for (const auto& path : candidatePaths) {
            file.open(path);
            if (file.is_open()) {
                Logger::Log(Logger::Level::INFO, "Logic",
                            "Loaded wordlist from: %s", path.c_str());
                break;
            }
        }

        if (!file.is_open()) {
            Logger::Log(Logger::Level::ERR, "Logic",
                        "Failed to find wordlist in any path");
            return false;
        }

        ReadWordList(file);
        file.close();

        Logger::Log(Logger::Level::INFO, "Logic",
                    "Loaded %zu words into wordlist cache",
                    g_state.cachedWordList.size());

        return !g_state.cachedWordList.empty();
    } catch (...) {
        g_state.cachedWordList.clear();
        return false;
    }
}

float CalculateRequiredEntropy() {
    float bits = 0.0f;

    switch (g_state.outputFormat) {
        case FORMAT_DECIMAL:
            bits = g_state.decimalDigits * 3.321928f;
            break;

        case FORMAT_INTEGER:
        {
            bool swapped = g_state.integerMin > g_state.integerMax;
            long long low = swapped ? g_state.integerMax : g_state.integerMin;
            long long high = swapped ? g_state.integerMin : g_state.integerMax;
            unsigned long long range = (unsigned long long)high - (unsigned long long)low + 1;
            if (range == 0) bits = 64.0f;
            else if (range <= 1) bits = 1.0f;
            else bits = std::ceil(std::log2((double)range));
            break;
        }

        case FORMAT_RANDOM_CHARACTERS:
        {
            int characterSetSize = 0;
            if (g_state.includeNumbers) characterSetSize += 10;
            if (g_state.includeUppercase) characterSetSize += 26;
            if (g_state.includeLowercase) characterSetSize += 26;
            if (g_state.includeSpecial) characterSetSize += 32;

            if (characterSetSize < 1) characterSetSize = 1;
            bits = g_state.randomCharactersLength * std::log2((double)characterSetSize);
            break;
        }

        case FORMAT_RAW_DATA:
            bits = (float)CSPRNG::GetRawDataLayout(
                       g_state.rawDataEncoding,
                       CSPRNG::RawDataBitCount(g_state.rawDataAmount, g_state.rawDataUnit))
                       .bitsCarried;
            break;

        case FORMAT_PASSPHRASE:
            bits = g_state.passphraseWordCount * PASSPHRASE_BITS_PER_WORD;
            break;

        case FORMAT_ONE_TIME_PAD:
            if (g_state.oneTimePadInputMode == ONE_TIME_PAD_TEXT_INPUT) {
                bits = strlen(g_state.oneTimePadMessage.data()) *
                       std::log2((double)CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE);
            } else if (g_state.oneTimePadInputMode == ONE_TIME_PAD_MESSAGE_LENGTH_INPUT) {
                bits = g_state.oneTimePadMessageLength *
                       std::log2((double)CSPRNG::ONE_TIME_PAD_ALPHABET_SIZE);
            } else {
                bits = (float)g_state.oneTimePadFileSize * 8.0f;
            }
            break;

        case FORMAT_UUID_V4:
            bits = 122.0f;
            break;

        case FORMAT_UUID_V7:
            bits = (g_state.uuidV7Ordering == UUIDV7_FULLY_RANDOM) ? 74.0f : 73.0f;
            break;
    }

    if (g_state.outputFormat != FORMAT_ONE_TIME_PAD && g_state.outputQuantity > 1)
        bits *= (float)g_state.outputQuantity;

    return (bits < 512.0f) ? 512.0f : bits;
}

void UpdateTargetEntropy() {
    g_state.targetEntropyBits = CalculateRequiredEntropy();
}

float CalculateEntropyFromDeltas(const std::vector<uint64_t>& deltas) {
    std::vector<uint8_t> bytes;
    bytes.reserve(deltas.size() * 2);

    for (uint64_t delta : deltas) {
        if (delta != 0) {
            bytes.push_back(static_cast<uint8_t>(delta & 0xFF));
            bytes.push_back(static_cast<uint8_t>((delta >> 8) & 0xFF));
        }
    }

    return Entropy::CalculateShannonEntropyBits(bytes);
}
