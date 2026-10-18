#pragma once
#include <vector>
#include <windows.h>

namespace Crypto {

template <typename T>
void SecureClearVector(std::vector<T>& vec) {
    if (!vec.empty()) {
        SecureZeroMemory(vec.data(), vec.size() * sizeof(T));
        vec.clear();
    }
}

}
