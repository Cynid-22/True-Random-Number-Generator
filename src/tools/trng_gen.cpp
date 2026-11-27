static constexpr int THREAD_USAGE_PERCENT = 50;

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#endif

#include "../crypto/hmac_drbg.h"
#include "../platform/os_random.h"

static constexpr size_t OS_RANDOM_SEED_BYTES = 64;

template <typename T>
static void AppendRawBytes(std::vector<uint8_t>& bytes, const T& value) {
    const uint8_t* valueBytes = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), valueBytes, valueBytes + sizeof(T));
}

static std::vector<uint8_t> CollectHardwareSeed() {
    std::vector<uint8_t> seed;
    seed.reserve(256);

    for (int i = 0; i < 16; i++) {
        uint64_t timestampCounter = __rdtsc();
        AppendRawBytes(seed, timestampCounter);
        volatile uint64_t busyWork = 0;
        for (int j = 0; j < (i + 1) * 137; j++) busyWork += j * timestampCounter;
    }

    LARGE_INTEGER performanceCounter;
    QueryPerformanceCounter(&performanceCounter);
    AppendRawBytes(seed, performanceCounter);

    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    AppendRawBytes(seed, nanoseconds);

    uint64_t tickCount = GetTickCount64();
    AppendRawBytes(seed, tickCount);

    uint32_t processId = GetCurrentProcessId();
    uint32_t threadId = GetCurrentThreadId();
    AppendRawBytes(seed, processId);
    AppendRawBytes(seed, threadId);

    std::array<uint8_t, OS_RANDOM_SEED_BYTES> osRandom{};
    if (Platform::FillOsRandom(osRandom.data(), osRandom.size()))
        seed.insert(seed.end(), osRandom.begin(), osRandom.end());
    SecureZeroMemory(osRandom.data(), osRandom.size());

    return seed;
}

static const char GENERATOR_PERSONALIZATION[] = "TRNG-GEN";

static std::vector<uint8_t> GenerateChunk(const std::vector<uint8_t>& entropyBytes,
                                          size_t byteCount,
                                          uint64_t chunkNumber) {
    uint8_t nonce[sizeof(chunkNumber)];
    for (size_t i = 0; i < sizeof(chunkNumber); i++)
        nonce[i] = static_cast<uint8_t>(chunkNumber >> (i * 8));
    Crypto::HmacDrbg generator(
        {entropyBytes.data(), entropyBytes.size()}, {nonce, sizeof(nonce)},
        {reinterpret_cast<const uint8_t*>(GENERATOR_PERSONALIZATION),
         sizeof(GENERATOR_PERSONALIZATION) - 1});

    std::vector<uint8_t> chunk(byteCount);
    size_t written = 0;
    while (written < byteCount) {
        const size_t requestBytes =
            std::min(byteCount - written, Crypto::HmacDrbg::MAX_BYTES_PER_REQUEST);
        if (!generator.Generate(chunk.data() + written, requestBytes)) {
            fprintf(stderr, "HMAC_DRBG refused a request\n");
            std::abort();
        }
        written += requestBytes;
    }
    return chunk;
}

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    fprintf(stderr, "trng_gen: test stream for statistical test suites only. "
                    "Do not use it for keys, passwords or other secrets.\n");

    static char outputBuffer[4 * 1024 * 1024];
    setvbuf(stdout, outputBuffer, _IOFBF, sizeof(outputBuffer));

    std::vector<uint8_t> seed = CollectHardwareSeed();

    const size_t CHUNK_SIZE = 4 * 1024 * 1024;
    const int totalCores = std::max(1, (int)std::thread::hardware_concurrency());
    const int threadCount = std::max(1, totalCores * THREAD_USAGE_PERCENT / 100);
    uint64_t chunkCounter = 0;

    std::vector<std::vector<uint8_t>> batchA(threadCount), batchB(threadCount);

    auto generateBatch = [&](std::vector<std::vector<uint8_t>>& batch) {
        std::vector<std::thread> threads;
        std::vector<uint64_t> chunkNumbers(threadCount);
        for (int t = 0; t < threadCount; t++) {
            chunkCounter++;
            chunkNumbers[t] = chunkCounter;
        }
        for (int t = 0; t < threadCount; t++) {
            threads.emplace_back([&seed, &batch, t, chunkNumber = chunkNumbers[t], CHUNK_SIZE]() {
                std::vector<uint8_t> chunkSeed = seed;
                for (int i = 0; i < 8; i++)
                    chunkSeed.push_back(static_cast<uint8_t>(chunkNumber >> (i * 8)));
                uint64_t timestampCounter = __rdtsc();
                AppendRawBytes(chunkSeed, timestampCounter);
                batch[t] = GenerateChunk(chunkSeed, CHUNK_SIZE, chunkNumber);
                SecureZeroMemory(chunkSeed.data(), chunkSeed.size());
            });
        }
        for (auto& thread : threads) thread.join();
    };

    auto writeBatch = [&](std::vector<std::vector<uint8_t>>& batch) -> bool {
        for (int t = 0; t < threadCount; t++) {
            size_t written = fwrite(batch[t].data(), 1, batch[t].size(), stdout);
            if (written != batch[t].size() || ferror(stdout)) return false;
            SecureZeroMemory(batch[t].data(), batch[t].size());
        }
        return true;
    };

    generateBatch(batchA);

    while (true) {
        std::thread backgroundB([&]() { generateBatch(batchB); });
        bool ok = writeBatch(batchA);
        backgroundB.join();
        if (!ok) break;

        std::thread backgroundA([&]() { generateBatch(batchA); });
        ok = writeBatch(batchB);
        backgroundA.join();
        if (!ok) break;
    }

    SecureZeroMemory(seed.data(), seed.size());
    return 0;
}
