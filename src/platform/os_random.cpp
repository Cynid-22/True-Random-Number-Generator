#include "os_random.h"
#include <windows.h>
#include <bcrypt.h>

namespace Platform {

namespace {

using GenRandomFunction = decltype(&BCryptGenRandom);

constexpr ULONG MAX_BYTES_PER_CALL = ~0UL;

GenRandomFunction LoadGenRandom() {
    HMODULE bcrypt = LoadLibraryExW(L"bcrypt.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!bcrypt)
        return nullptr;
    return reinterpret_cast<GenRandomFunction>(GetProcAddress(bcrypt, "BCryptGenRandom"));
}

}

bool FillOsRandom(uint8_t* output, size_t length) {
    static const GenRandomFunction genRandom = LoadGenRandom();
    if (!genRandom || length > MAX_BYTES_PER_CALL)
        return false;
    return BCRYPT_SUCCESS(genRandom(nullptr, output, static_cast<ULONG>(length),
                                    BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

}
