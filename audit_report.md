# TRNG Codebase Security & Feature Audit Report

**Date**: 2026-01-31
**Status**: Completed
**Auditor**: Antigravity

## Executive Summary
The codebase is structured professionally with strong adherence to the specified cryptographic standards (SHA-512, HKDF, ChaCha20, AES-256). Memory hygiene (`SecureZeroMemory`) is implemented rigorously across all cryptographic and entropy collection modules. The "Quad-Layer" architecture logic is implemented as described in the requirements.

However, **two high-severity issues** were identified that compromise the "True Randomness" and "Chronological Mixing" guarantees of the system.

---

## 1. High Severity Issues

### A. Modulo Bias in Output Generation (Predictability Vulnerability)
**Location**: `src/logic/csprng.cpp` -> `GenerateInteger`, `GenerateDecimal`, `GenerateCustomString`.
**Description**: The code uses the modulo operator (`%`) directly on random data to map it to a specific range (e.g., `val % 10` for decimal digits, `val % range` for integers).
-   **Vulnerability**: When the source range (e.g., 256 for a byte, 65536 for uint16) is not a perfect multiple of the target range (e.g., 10), the lower values in the target range appear slightly more frequently.
-   **Impact**: This introduces statistical bias. For a "Top Secret" grade TRNG, this violates the requirement of uniformity. Attackers can exploit this bias in large output samples to distinguish the output from true randomness.
-   **Recommendation**: Implement **Rejection Sampling**. Discard values that fall into the "remainder" zone of the source range to ensure every target value has an exactly equal probability.

### B. Timestamp Epoch Inconsistency (Mixing Logic Failure)
**Location**: `src/entropy/mouse/mouse.cpp` vs All Other Collectors.
**Description**:
-   `MouseCollector` uses `kernelTime` from the Windows Hook (which is based on System Uptime/`GetTickCount`). It converts this to nanoseconds: `kernelTime * 1000000ULL`.
-   All other collectors (`Microphone`, `Keystroke`, `CpuJitter`, `ClockDrift`) use `Entropy::GetNanosecondTimestamp()` (which wraps `std::chrono::high_resolution_clock`/`QueryPerformanceCounter`).
-   **Vulnerability**: These two clocks use different epochs (start times) and drift. Mouse events will likely have timestamps that are wildly different (potentially by hours or decades depending on implementation) from other events.
-   **Impact**: The `EntropyPool` sorts data chronologically. The mismatch means Mouse data will not be correctly interleaved with other sources, destroying the "Chronological Mixing" logic required by the Quad-Layer architecture.
-   **Recommendation**: Consistently use `Entropy::GetNanosecondTimestamp()` for all events, or normalize the Mouse `kernelTime` to the high-resolution clock epoch.

---

## 2. Feature & Logic Mismatches

### C. Entropy Estimation Heuristic
**Location**: `src/entropy/pool.cpp` -> `GetTotalBits`.
**Description**: The function calculates total pooled bits using a heuristic (64 bits for Mic, 2 bits for others) rather than the "Shannon Entropy" calculation requested in `task.md` (Item 51/133).
-   **Note**: `CalculateShannonEntropy` exists in `logic.cpp`, but it is not used for the primary "Total Entropy" progress bar in the UI.
-   **Recommendation**: Clarify if the UI bar should use the conservative heuristic (safe default) or the real-time Shannon calculation.

### D. Keystroke "Flight Time" Implementation
**Location**: `src/entropy/keystroke/keystroke.cpp`.
**Description**: The code captures the delta between events effectively. A comment suggests "flight time" isn't strictly calculated as "KeyUp to Next KeyDown", but the implementation `flightTime = timestamp - m_lastKeyUpTime` (Line 134) **correctly implements** the requirement. The comment on lines 123-128 is slightly misleading but the code is correct.
-   **Status**: False alarm in comments. Code is correct.

---

## 3. Cryptographic & Memory Safety Checks

| Component | Status | Notes |
| :--- | :--- | :--- |
| **AES-256** | **PASS** | Valid logic, `SecureZeroMemory` on keys/iv/state. |
| **ChaCha20** | **PASS** | `InitState` matches RFC 8439 (96-bit nonce). Memory safe. |
| **SHA-512** | **PASS** | Padding/Endianness correct. State wiped. |
| **HKDF** | **PASS** | Correct HMAC-extract/expand logic. |
| **Quad-Layer** | **PASS** | Flow (ChaCha->XOR->AES->ChaCha) is verified. |
| **Lock-In** | **PASS** | Logic in `GenerateOutput` respects timestamp locking. |

## 4. Nitpicks & Best Practices

1.  **Endianness Assumption**: `AES256::AddRoundKey` assumes specific byte layout for words. While likely correct on Windows (Little Endian), explicit endian-safe extraction is safer.
2.  **Mouse Hook Type**: `WH_MOUSE_LL` is a global hook. It injects into the message loop. This is standard for global input, but requires the app to remain responsive to prevent system-wide lag (handled correctly by threading).
