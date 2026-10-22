#include "hmac_drbg.h"
#include <algorithm>
#include <cstring>
#include <windows.h>

namespace Crypto {

static const uint8_t INITIAL_KEY[SHA512::HASH_SIZE] = {};
static constexpr uint8_t INITIAL_VALUE_BYTE = 0x01;
static constexpr uint8_t FIRST_ROUND_SEPARATOR = 0x00;
static constexpr uint8_t SECOND_ROUND_SEPARATOR = 0x01;

HmacDrbg::HmacDrbg(ByteSpan entropyInput, ByteSpan nonce, ByteSpan personalization)
    : m_key(INITIAL_KEY, sizeof(INITIAL_KEY)) {
    m_value.fill(INITIAL_VALUE_BYTE);
    Update({entropyInput, nonce, personalization});
}

HmacDrbg::~HmacDrbg() {
    SecureZeroMemory(m_value.data(), m_value.size());
    m_reseedCounter = 0;
}

void HmacDrbg::Reseed(ByteSpan entropyInput, ByteSpan additionalInput) {
    Update({entropyInput, additionalInput});
    m_reseedCounter = 1;
}

bool HmacDrbg::Generate(uint8_t* output, size_t byteCount, ByteSpan additionalInput) {
    if (byteCount > MAX_BYTES_PER_REQUEST || m_reseedCounter > MAX_REQUESTS_BETWEEN_RESEEDS)
        return false;

    if (additionalInput.length > 0)
        Update({additionalInput});

    size_t written = 0;
    while (written < byteCount) {
        m_value = m_key.Mac(m_value.data(), m_value.size());
        const size_t copied = std::min(byteCount - written, m_value.size());
        memcpy(output + written, m_value.data(), copied);
        written += copied;
    }

    Update({additionalInput});
    m_reseedCounter++;
    return true;
}

void HmacDrbg::Update(std::initializer_list<ByteSpan> providedData) {
    UpdateRound(FIRST_ROUND_SEPARATOR, providedData);
    const bool hasProvidedData =
        std::any_of(providedData.begin(), providedData.end(),
                    [](const ByteSpan& part) { return part.length > 0; });
    if (hasProvidedData)
        UpdateRound(SECOND_ROUND_SEPARATOR, providedData);
}

void HmacDrbg::UpdateRound(uint8_t separator, std::initializer_list<ByteSpan> providedData) {
    SHA512::Hasher message = m_key.BeginMessage();
    message.Update(m_value.data(), m_value.size());
    message.Update(&separator, 1);
    for (const ByteSpan& part : providedData) {
        message.Update(part.data, part.length);
    }
    SHA512::Hash newKey = m_key.FinishMessage(message);
    m_key = SHA512::HMACKey(newKey.data(), newKey.size());
    SecureZeroMemory(newKey.data(), newKey.size());
    m_value = m_key.Mac(m_value.data(), m_value.size());
}

}
