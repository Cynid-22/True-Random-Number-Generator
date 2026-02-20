# Security Audit Fixes Implementation Plan

## Goal
Address all findings from the Security Audit Report (`audit_report_final.md`), focusing on cryptographic logic correctness and memory hygiene.

## Proposed Changes

### 1. Memory Hygiene Helper
- **[NEW] `src/crypto/secure_mem.h`**
    - Create a template function `SecureClearVector<T>` that calls `SecureZeroMemory` on the vector's data before clearing it.
    - This will be used to replace standard `.clear()` calls on sensitive data vectors and to ensure local vectors are wiped before destruction.

### 2. Cryptographic Logic (Layer 2 Fix)
- **[MODIFY] `src/logic/csprng.cpp`**
    - Update the "Layer 2: Entropy Injection" loop.
    - **Current**: Iterates `stream1.size()` times.
    - **Fix**: Iterate `entropyBytes.size()` times (if greater than stream size), modulo `stream1.size()`.
    - `stream1[i % stream1.size()] ^= entropyBytes[i]`
    - This ensures every byte of the entropy pool affects the output stream (Mixes full pool).

### 3. Memory Hygiene Fixes (Collectors)
- **[MODIFY] `src/entropy/mouse/mouse.cpp`**
    - Update `FlushLocalBuffer` to use `SecureClearVector`.
- **[MODIFY] `src/entropy/microphone/microphone.cpp`**
    - Update `CaptureThread` to use `SecureClearVector` for `newPoints`.
- **[MODIFY] `src/entropy/keystroke/keystroke.cpp`**
    - Update `OnKeyDown`/`OnKeyUp` (if applicable) or just ensure `SecureClearVector` is used in `SecureClearBuffer`.

### 4. Memory Hygiene Fixes (CSPRNG & AppState)
- **[MODIFY] `src/logic/csprng.cpp`**
    - In `GenerateOutput`, explicitly call `SecureClearVector` on `pooledData` before it goes out of scope.
    - In `GenerateOutput`, explicitly call `SecureClearVector` on `randomBytes` (already done, but double check).
- **[MODIFY] `src/core/app_state.h`**
    - Add a destructor to `AppState` to securely wipe `generatedOutput` and `otpMessage` on application shutdown.

## Verification Plan
### Automated Verification
- No new unit tests planned (no test framework in context).
- Will rely on code review and manual verification.

### Manual Verification
- **Layer 2 Logic**: Verify code changes ensure full pool usage.
- **Memory**: Verify all `std::vector`s holding sensitive data are wiped.
