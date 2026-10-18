static constexpr int THREAD_USAGE_PERCENT = 50;

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#endif

#include "../crypto/quad_layer.h"

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

    return seed;
}

static std::vector<uint8_t> GenerateChunk(const std::vector<uint8_t>& entropyBytes,
                                          size_t byteCount,
                                          uint64_t chunkNumber) {
    std::string infoText = "TRNG-GEN|C:" + std::to_string(chunkNumber);
    std::vector<uint8_t> info(infoText.begin(), infoText.end());
    return Crypto::GenerateQuadLayerStream(entropyBytes, info, byteCount);
}

int main() {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

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
