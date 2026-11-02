#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>
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

template <typename T>
class SecureBox {
public:
    SecureBox() = default;

    SecureBox(const SecureBox& other) {
        if (other.m_object)
            Emplace(*other.m_object);
    }

    SecureBox(SecureBox&& other) noexcept : m_object(std::exchange(other.m_object, nullptr)) {}

    SecureBox& operator=(const SecureBox& other) {
        if (this != &other) {
            Reset();
            if (other.m_object)
                Emplace(*other.m_object);
        }
        return *this;
    }

    SecureBox& operator=(SecureBox&& other) noexcept {
        if (this != &other) {
            Reset();
            m_object = std::exchange(other.m_object, nullptr);
        }
        return *this;
    }

    ~SecureBox() { Reset(); }

    template <typename... Args>
    T& Emplace(Args&&... args) {
        Reset();
        SecureAllocator<T> allocator;
        T* memory = allocator.allocate(1);
        try {
            m_object = new (memory) T(std::forward<Args>(args)...);
        } catch (...) {
            allocator.deallocate(memory, 1);
            throw;
        }
        return *m_object;
    }

    void Reset() noexcept {
        if (!m_object)
            return;
        m_object->~T();
        SecureAllocator<T>().deallocate(m_object, 1);
        m_object = nullptr;
    }

    explicit operator bool() const noexcept { return m_object != nullptr; }
    T& operator*() noexcept { return *m_object; }
    const T& operator*() const noexcept { return *m_object; }
    T* operator->() noexcept { return m_object; }
    const T* operator->() const noexcept { return m_object; }

private:
    T* m_object = nullptr;
};

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
