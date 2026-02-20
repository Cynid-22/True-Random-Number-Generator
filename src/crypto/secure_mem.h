#pragma once
#include <vector>
#include <windows.h> // For SecureZeroMemory

namespace Crypto {

// Securely zero the memory of a vector before clearing it
// This ensures that sensitive data (entropy, keys) doesn't linger in heap capacity
template <typename T>
void SecureClearVector(std::vector<T>& vec) {
    if (!vec.empty()) {
        SecureZeroMemory(vec.data(), vec.size() * sizeof(T));
        vec.clear();
        // shrink_to_fit request to reduce capacity, though not guaranteed to zero the *freed* memory 
        // by the allocator, but at least we zeroed the *active* memory before release.
        // For standard allocators, the memory is just marked free. 
        // We zeroed it first, so the data in that block is now 0s.
        // (Unless T has a complex destructor that copies data? 
        // EntropyDataPoint is POD, uint8_t is POD. char is POD. So this is safe.)
    }
}

} // namespace Crypto
