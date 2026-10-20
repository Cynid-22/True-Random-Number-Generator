#include "quad_layer.h"
#include <algorithm>
#include <windows.h>
#include "aes.h"
#include "chacha20.h"
#include "hkdf.h"
#include "sha512.h"

namespace Crypto {

static std::vector<uint8_t> ChaCha20StreamFromSeed(const SHA512::Hash& seed,
                                                   const std::vector<uint8_t>& info,
                                                   size_t byteCount) {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> seedBytes(seed.begin(), seed.end());
    std::vector<uint8_t> keyMaterial = HKDF::DeriveKey(seedBytes, salt, info, 44);
    SecureZeroMemory(seedBytes.data(), seedBytes.size());

    ChaCha20::Key key;
    ChaCha20::Nonce nonce;
    std::copy(keyMaterial.begin(), keyMaterial.begin() + 32, key.begin());
    std::copy(keyMaterial.begin() + 32, keyMaterial.begin() + 44, nonce.begin());
    SecureZeroMemory(keyMaterial.data(), keyMaterial.size());

    std::vector<uint8_t> stream = ChaCha20::GenerateStream(key, nonce, byteCount);

    SecureZeroMemory(key.data(), key.size());
    SecureZeroMemory(nonce.data(), nonce.size());
    return stream;
}

std::vector<uint8_t> GenerateQuadLayerStream(const std::vector<uint8_t>& entropyBytes,
                                             const std::vector<uint8_t>& layerOneInfo,
                                             size_t byteCount) {
    return GenerateQuadLayerStream(entropyBytes.data(), entropyBytes.size(), layerOneInfo,
                                   byteCount);
}

std::vector<uint8_t> GenerateQuadLayerStream(const uint8_t* entropyBytes,
                                             size_t entropyByteCount,
                                             const std::vector<uint8_t>& layerOneInfo,
                                             size_t byteCount) {
    SHA512::Hash masterSeed = SHA512::Compute(entropyBytes, entropyByteCount);
    std::vector<uint8_t> maskedStream = ChaCha20StreamFromSeed(masterSeed, layerOneInfo, byteCount);

    if (entropyByteCount > 0) {
        size_t foldLength = std::max(maskedStream.size(), entropyByteCount);
        for (size_t i = 0; i < foldLength; i++) {
            maskedStream[i % maskedStream.size()] ^= entropyBytes[i % entropyByteCount];
        }
    }

    SHA512::Hash maskedStreamHash = SHA512::Compute(maskedStream);
    std::vector<uint8_t> aesKey(maskedStreamHash.begin(), maskedStreamHash.begin() + 32);
    std::vector<uint8_t> aesIv(maskedStreamHash.begin() + 32, maskedStreamHash.begin() + 48);
    std::vector<uint8_t> aesStream = AES256::EncryptCTR(aesKey, aesIv, maskedStream);

    SHA512::Hash aesStreamHash = SHA512::Compute(aesStream);
    const std::vector<uint8_t> layerFourInfo = {'L', 'A', 'Y', 'E', 'R', '4'};
    std::vector<uint8_t> result = ChaCha20StreamFromSeed(aesStreamHash, layerFourInfo, byteCount);

    SecureZeroMemory(masterSeed.data(), masterSeed.size());
    SecureZeroMemory(maskedStream.data(), maskedStream.size());
    SecureZeroMemory(maskedStreamHash.data(), maskedStreamHash.size());
    SecureZeroMemory(aesKey.data(), aesKey.size());
    SecureZeroMemory(aesIv.data(), aesIv.size());
    SecureZeroMemory(aesStream.data(), aesStream.size());
    SecureZeroMemory(aesStreamHash.data(), aesStreamHash.size());

    return result;
}

}
