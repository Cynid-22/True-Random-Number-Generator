# TRNG Security Audit Report

## Executive Summary
This report details the findings of a comprehensive security audit of the True Random Number Generator (TRNG) codebase. The audit focuses on feature completeness, security vulnerabilities, memory safety, and compliance with project standards (`task.md`, `audit.md`, `standards.md`).

**Overall Status**: The system is functional and implements the core security requirements (Quad-Layer CSPRNG, Entropy Pooling, Logging Hardening). However, several **High** and **Medium** severity issues regarding memory hygiene and strict cryptographic logic adherence have been identified.

## Audit Checklist Findings

| Category | Item | Status | Notes |
| :--- | :--- | :--- | :--- |
| **Crypto** | **Quad-Layer Arch** | **PARTIAL** | Core layers present. Layer 2 logic has a minor deficiency for short outputs. |
| **Crypto** | ChaCha20 Masking | PASS | Correctly seeded with HKDF + formatting context. |
| **Crypto** | Entropy Injection | **WARN** | XOR Fold loop relies on output stream length; may not use full pool for short outputs. |
| **Crypto** | AES-256 Trans | PASS | CTR mode, Dynamic Keying from SHA-512 of previous stream. |
| **Crypto** | ChaCha20 Whitening | PASS | Re-seeded from SHA-512 of AES output. |
| **Memory** | Secure Erasure | **FAIL** | `SecureZeroMemory` used extensively but `std::vector` destructors leak sensitive data in stack/heap. |
| **Memory** | Entropy Pool Wiping | PASS | `EntropyPool::Clear` and `SecureWipe` implementation is correct. |
| **Entropy** | Source Diversity | PASS | Mic, Mouse, Keyboard, Jitter, Clock Drift implemented. |
| **Entropy** | Lock-In Mechanism | PASS | Logic correctly separates Locked vs New entropy based on timestamps. |
| **OpSec** | No Sensitive Logs | PASS | Debug logging removed. Sensitive buffers not logged. |

## Detailed Findings

### 1. Cryptographic Logic: Layer 2 Partial Pool Usage
**Severity: MEDIUM**
- **Location**: `src/logic/csprng.cpp` (Line 123)
- **Issue**: The Entropy Injection layer iterates over the *generated stream* length (`stream1.size()`) to XOR in the entropy pool.
  ```cpp
  for (size_t i = 0; i < stream1.size(); i++) {
      stream1[i] ^= entropyBytes[i % entropyBytes.size()];
  }
  ```
- **Impact**: If the requested output size (and thus `stream1`) is smaller than the collected entropy pool, only the first `N` bytes of the entropy pool are injected. The remaining pool data is ignored in this specific layer.
- **Requirement**: `audit.md` requires: "Ensure the loop cycles through the *entire* entropy pool".
- **Mitigation**: While Layer 1 (Masking) already hashes the *entire* pool into the seed (providing computational security), Layer 2 is intended for Information Theoretic security (OTP property). For true OTP, the key (entropy) must be used. If output is short, this is less critical, but strict adherence suggests hashing or folding the entire pool down to the stream size if `pool > stream`.

### 2. Memory Hygiene: `std::vector` Destructor Leakage
**Severity: HIGH**
- **Location**: Multiple (`csprng.cpp`, `mouse.cpp`, `microphone.cpp`)
- **Issue**: The codebase relies on `std::vector` for passing sensitive `EntropyDataPoint` structs and byte buffers. While `SecureZeroMemory` is called on *some* buffers before clearing, `std::vector`'s destructor **does not** wipe memory when objects go out of scope.
- **Specific Instances**:
  1.  `CSPRNG::GenerateOutput`: `std::vector<EntropyDataPoint> pooledData` is a local copy of the pool. It goes out of scope without wiping.
  2.  `MicrophoneCollector::CaptureThread`: `std::vector<EntropyDataPoint> newPoints` is created and destroyed inside the loop without wiping.
  3.  `EntropyPool::GetPooledData`: Returns a vector by value. The temporary return object is destroyed without wiping.
  4.  `g_state.generatedOutput`: Global vector not explicitly wiped on application exit (relying on OS memory reclamation).
- **Recommendation**: Implement a `SecureVector<T>` wrapper or custom allocator that zeros memory on deallocation, or manually call `SecureZeroMemory` on every vector before it goes out of scope.

### 3. Mouse Collection: Buffer Clearing without Wiping
**Severity: MEDIUM**
- **Location**: `src/entropy/mouse/mouse.cpp` (Line 99 `FlushLocalBuffer`)
- **Issue**: `m_localBuffer.clear()` is called to empty the batch buffer.
  ```cpp
  m_buffer.insert(..., m_localBuffer.begin(), ...);
  m_localBuffer.clear(); // Does not zero memory
  ```
- **Impact**: The sensitive mouse coordinates remain in the `capacity` of the vector until overwritten.
- **Recommendation**: Call `SecureZeroMemory(m_localBuffer.data(), ...)` before `clear()`.

### 4. Application State: Global Output Buffer
**Severity: LOW**
- **Location**: `src/core/app_state.h`
- **Issue**: `g_state.generatedOutput` holds the plain-text random output. It is a global variable.
- **Impact**: Code assumes "Clearing" via GUI is the only lifecycle end. If the app crashes or closes, this buffer is not actively wiped.
- **Recommendation**: Add a destructor to `AppState` struct that calls `SecureZeroMemory` on its members.

### 5. Formatting & Cleanup
**Severity: NITPICK**
- `csprng.cpp` `GenerateOutput` returns `GenerationResult` by value. The struct contains `std::vector<char> output`. The compiler likely optimizes this (RVO), but if a copy occurs, a temporary vector containing the secret output is created and destroyed without wiping.

## Conclusion
The TRNG core is robust, but the extensive use of C++ STL containers (`std::vector`) without custom secure allocators introduces systematic memory hygiene weaknesses. The cryptographic logic is sound, with one minor deviation in Layer 2 regarding loop bounds for short outputs.

**Next Steps:**
1.  **URGENT**: Patch `MicrophoneCollector` and `MouseCollector` to securely wipe local vectors.
2.  **URGENT**: Update `CSPRNG::GenerateOutput` to securely wipe `pooledData`.
3.  **Refactor**: Introduce a `SecureClear(std::vector&)` helper and use it on all intermediate vectors.
4.  **Logic Fix**: Update Layer 2 XOR loop to ensure full pool entropy is folded in (e.g., hash pool to stream length if pool > stream, or fold iteratively).

Report Generated: 2026-01-31
