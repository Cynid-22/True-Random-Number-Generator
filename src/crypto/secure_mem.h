#pragma once
#include <algorithm>
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

template <typename T>
void ReserveWithoutLeavingCopies(std::vector<T>& vec, size_t extraElements) {
    size_t needed = vec.size() + extraElements;
    if (needed <= vec.capacity())
        return;
    std::vector<T> larger;
    larger.reserve(std::max(needed, vec.capacity() * 2));
    larger.insert(larger.end(), vec.begin(), vec.end());
    SecureClearVector(vec);
    vec.swap(larger);
}

}
