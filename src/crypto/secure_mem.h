#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>
#include <windows.h>

namespace Crypto {

template <typename T>
struct SecureAllocator {
    using value_type = T;

    SecureAllocator() noexcept = default;

    template <typename U>
    SecureAllocator(const SecureAllocator<U>&) noexcept {}

    T* allocate(size_t count) {
        if (count > SIZE_MAX / sizeof(T))
            throw std::bad_array_new_length();
        const size_t byteCount = std::max<size_t>(count, 1) * sizeof(T);
        void* memory = VirtualAlloc(nullptr, byteCount, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!memory)
            throw std::bad_alloc();
        VirtualLock(memory, byteCount);
        return static_cast<T*>(memory);
    }

    void deallocate(T* memory, size_t count) noexcept {
        if (!memory)
            return;
        const size_t byteCount = std::max<size_t>(count, 1) * sizeof(T);
        SecureZeroMemory(memory, byteCount);
        VirtualUnlock(memory, byteCount);
        VirtualFree(memory, 0, MEM_RELEASE);
    }
};

template <typename T, typename U>
bool operator==(const SecureAllocator<T>&, const SecureAllocator<U>&) noexcept {
    return true;
}

template <typename T, typename U>
bool operator!=(const SecureAllocator<T>&, const SecureAllocator<U>&) noexcept {
    return false;
}

using SecureBytes = std::vector<uint8_t, SecureAllocator<uint8_t>>;

template <typename T, typename Allocator>
void SecureClearVector(std::vector<T, Allocator>& vec) {
    if (!vec.empty()) {
        SecureZeroMemory(vec.data(), vec.size() * sizeof(T));
        vec.clear();
    }
}

template <typename T, typename Allocator>
void ReserveWithoutLeavingCopies(std::vector<T, Allocator>& vec, size_t extraElements) {
    size_t needed = vec.size() + extraElements;
    if (needed <= vec.capacity())
        return;
    std::vector<T, Allocator> larger;
    larger.reserve(std::max(needed, vec.capacity() * 2));
    larger.insert(larger.end(), vec.begin(), vec.end());
    SecureClearVector(vec);
    vec.swap(larger);
}

}
