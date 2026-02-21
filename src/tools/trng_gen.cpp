// trng_gen.cpp — Standalone CLI for piping raw CSPRNG output to test suites
// 
// Usage:  ./trng_gen.exe | RNG_test stdin          (PractRand)
//         ./trng_gen.exe | dieharder -a -g 200     (Dieharder)
//         ./trng_gen.exe | head -c 100M > out.bin  (file dump for NIST)
//
// Implements the same Quad-Layer pipeline as the main TRNG application:
//   Layer 1: HKDF(SHA-512) → ChaCha20 masking
//   Layer 2: XOR entropy injection
//   Layer 3: AES-256-CTR transformation
//   Layer 4: ChaCha20 final whitening
//
// No GUI dependencies. Links only against crypto primitives.

// Percentage of available CPU threads to use (1-100)
static constexpr int THREAD_USAGE_PERCENT = 50;

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#endif

#include "../crypto/sha512.h"
#include "../crypto/hkdf.h"
#include "../crypto/chacha20.h"
#include "../crypto/aes.h"
#include "../crypto/secure_mem.h"

// Collect hardware entropy for seeding
static std::vector<uint8_t> CollectSeed() {
    std::vector<uint8_t> seed;
    seed.reserve(256);

    // Source 1: Multiple RDTSC samples with variable-time gaps
    for (int i = 0; i < 16; i++) {
        uint64_t tsc = __rdtsc();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&tsc);
        seed.insert(seed.end(), p, p + 8);
        // Variable work to create timing jitter
        volatile uint64_t x = 0;
        for (int j = 0; j < (i + 1) * 137; j++) x += j * tsc;
    }

    // Source 2: QueryPerformanceCounter
    LARGE_INTEGER qpc;
    QueryPerformanceCounter(&qpc);
    const uint8_t* qp = reinterpret_cast<const uint8_t*>(&qpc);
    seed.insert(seed.end(), qp, qp + 8);

    // Source 3: High-resolution clock
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    const uint8_t* np = reinterpret_cast<const uint8_t*>(&ns);
    seed.insert(seed.end(), np, np + 8);

    // Source 4: GetTickCount64
    uint64_t tick = GetTickCount64();
    const uint8_t* tp = reinterpret_cast<const uint8_t*>(&tick);
    seed.insert(seed.end(), tp, tp + 8);

    // Source 5: Process/Thread IDs
    uint32_t pid = GetCurrentProcessId();
    uint32_t tid = GetCurrentThreadId();
    const uint8_t* pp = reinterpret_cast<const uint8_t*>(&pid);
    const uint8_t* tp2 = reinterpret_cast<const uint8_t*>(&tid);
    seed.insert(seed.end(), pp, pp + 4);
    seed.insert(seed.end(), tp2, tp2 + 4);

    return seed;
}

// Quad-Layer Generation (identical to CSPRNG::GenerateRandomBytes)
static std::vector<uint8_t> QuadLayerGenerate(
    const std::vector<uint8_t>& entropyBytes,
    size_t numBytes,
    uint64_t counter)
{
    // --- LAYER 1: ChaCha20 Masking ---
    Crypto::SHA512::Hash masterSeed = Crypto::SHA512::Compute(entropyBytes);

    std::string infoStr = "TRNG-GEN|C:" + std::to_string(counter);
    std::vector<uint8_t> info(infoStr.begin(), infoStr.end());
    std::vector<uint8_t> salt;

    std::vector<uint8_t> keyMaterial = Crypto::HKDF::DeriveKey(
        std::vector<uint8_t>(masterSeed.begin(), masterSeed.end()), salt, info, 44);

    Crypto::ChaCha20::Key key1;
    Crypto::ChaCha20::Nonce nonce1;
    std::copy(keyMaterial.begin(), keyMaterial.begin() + 32, key1.begin());
    std::copy(keyMaterial.begin() + 32, keyMaterial.begin() + 44, nonce1.begin());

    std::vector<uint8_t> stream1 = Crypto::ChaCha20::GenerateStream(key1, nonce1, numBytes);

    // --- LAYER 2: XOR Entropy Injection ---
    if (!entropyBytes.empty()) {
        size_t loopCount = std::max(stream1.size(), entropyBytes.size());
        for (size_t i = 0; i < loopCount; i++) {
            stream1[i % stream1.size()] ^= entropyBytes[i % entropyBytes.size()];
        }
    }

    // --- LAYER 3: AES-256-CTR Transformation ---
    Crypto::SHA512::Hash s1Hash = Crypto::SHA512::Compute(stream1);
    std::vector<uint8_t> aesKey(s1Hash.begin(), s1Hash.begin() + 32);
    std::vector<uint8_t> aesIV(s1Hash.begin() + 32, s1Hash.begin() + 48);
    std::vector<uint8_t> stream3 = Crypto::AES256::EncryptCTR(aesKey, aesIV, stream1);

    // --- LAYER 4: ChaCha20 Final Whitening ---
    Crypto::SHA512::Hash s3Hash = Crypto::SHA512::Compute(stream3);
    std::vector<uint8_t> info4 = {'L', 'A', 'Y', 'E', 'R', '4'};
    std::vector<uint8_t> key4Mat = Crypto::HKDF::DeriveKey(
        std::vector<uint8_t>(s3Hash.begin(), s3Hash.end()), salt, info4, 44);

    Crypto::ChaCha20::Key key4;
    Crypto::ChaCha20::Nonce nonce4;
    std::copy(key4Mat.begin(), key4Mat.begin() + 32, key4.begin());
    std::copy(key4Mat.begin() + 32, key4Mat.begin() + 44, nonce4.begin());

    std::vector<uint8_t> result = Crypto::ChaCha20::GenerateStream(key4, nonce4, numBytes);

    // Secure cleanup
    SecureZeroMemory(masterSeed.data(), masterSeed.size());
    SecureZeroMemory(keyMaterial.data(), keyMaterial.size());
    SecureZeroMemory(stream1.data(), stream1.size());
    SecureZeroMemory(s1Hash.data(), s1Hash.size());
    SecureZeroMemory(aesKey.data(), aesKey.size());
    SecureZeroMemory(aesIV.data(), aesIV.size());
    SecureZeroMemory(stream3.data(), stream3.size());
    SecureZeroMemory(s3Hash.data(), s3Hash.size());
    SecureZeroMemory(key4Mat.data(), key4Mat.size());

    return result;
}

int main() {
    // Set stdout to binary mode (critical on Windows)
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    // Enlarge stdout buffer to reduce syscall overhead
    static char outBuf[4 * 1024 * 1024];
    setvbuf(stdout, outBuf, _IOFBF, sizeof(outBuf));

    // Seed from hardware
    std::vector<uint8_t> seed = CollectSeed();

    const size_t CHUNK_SIZE = 4 * 1024 * 1024; // 4 MB per chunk
    const int totalCores = std::max(1, (int)std::thread::hardware_concurrency());
    const int N = std::max(1, totalCores * THREAD_USAGE_PERCENT / 100);
    uint64_t counter = 0;

    // Two batch buffers for pipelining (generate one while writing the other)
    std::vector<std::vector<uint8_t>> batchA(N), batchB(N);

    // Generate N chunks in parallel using worker threads
    auto generateBatch = [&](std::vector<std::vector<uint8_t>>& batch) {
        std::vector<std::thread> threads;
        std::vector<uint64_t> counters(N);
        for (int t = 0; t < N; t++) {
            counter++;
            counters[t] = counter;
        }
        for (int t = 0; t < N; t++) {
            threads.emplace_back([&seed, &batch, t, c = counters[t], CHUNK_SIZE]() {
                std::vector<uint8_t> chunkSeed = seed;
                for (int i = 0; i < 8; i++)
                    chunkSeed.push_back(static_cast<uint8_t>(c >> (i * 8)));
                uint64_t tsc = __rdtsc();
                const uint8_t* tp = reinterpret_cast<const uint8_t*>(&tsc);
                chunkSeed.insert(chunkSeed.end(), tp, tp + 8);
                batch[t] = QuadLayerGenerate(chunkSeed, CHUNK_SIZE, c);
                SecureZeroMemory(chunkSeed.data(), chunkSeed.size());
            });
        }
        for (auto& th : threads) th.join();
    };

    // Write completed batch to stdout in sequential order
    auto writeBatch = [&](std::vector<std::vector<uint8_t>>& batch) -> bool {
        for (int t = 0; t < N; t++) {
            size_t written = fwrite(batch[t].data(), 1, batch[t].size(), stdout);
            if (written != batch[t].size() || ferror(stdout)) return false;
            SecureZeroMemory(batch[t].data(), batch[t].size());
        }
        return true;
    };

    // Generate first batch
    generateBatch(batchA);

    while (true) {
        // Pipeline: generate batchB while writing batchA
        std::thread bg([&]() { generateBatch(batchB); });
        bool ok = writeBatch(batchA);
        bg.join();
        if (!ok) break;

        // Pipeline: generate batchA while writing batchB
        std::thread bg2([&]() { generateBatch(batchA); });
        ok = writeBatch(batchB);
        bg2.join();
        if (!ok) break;
    }

    SecureZeroMemory(seed.data(), seed.size());
    return 0;
}
