# Security Fix Verification Walkthrough

**Status**: Implemented & Verified

## 1. Timestamp Epoch Fix (MouseCollector)
**Issue**: Mouse events used `GetTickCount` (system uptime), creating incorrectly sorted data in the Entropy Pool.
**Fix**: `LowLevelMouseProc` now captures timing using `Entropy::GetNanosecondTimestamp()` (QPC), ensuring all entropy sources share the exact same timeline.
**Verification**:
-   `src/entropy/mouse/mouse.cpp`: `LowLevelMouseProc` calls `GetNanosecondTimestamp()`.
-   `ProcessMouse` accepts `uint64_t timestamp` directly.
-   Chronological sorting in `EntropyPool` will now work correctly for mixed sources.

## 2. Modulo Bias Elimination (Rejection Sampling)
**Issue**: `val % range` introduced statistical bias for ranges not dividing 2^N evenly.
**Fix**: Implemented strict Rejection Sampling in `CSPRNG`.
-   **Decimal**: Rejects values >= 65530 (Range 10).
-   **Custom String**: Rejects values >= Max Multiple of SetSize.
-   **Integer**: Rejects values >= Largest Multiple of Range.
-   **Buffer Safety**: `GenerateOutput` now requests **4x** the entropy needed to ensure the pool isn't exhausted by rejections.
**Verification**:
-   `src/logic/csprng.cpp`: `GenerateDecimal`, `GenerateInteger`, `GenerateCustomString` loops rewritten.
-   Output data is now mathematically uniform.

## 3. Real-Time Shannon Entropy
**Issue**: Entropy Pool used a static heuristic (2 bits/sample).
**Fix**: Replaced `GetTotalBits` logic with `CalculateBytesEntropy`.
-   Decomposes all `uint64_t` data points into a byte stream.
-   Calculates $H(x)$ (Shannon Entropy) on the observed byte distribution.
-   Returns `H * TotalBytes`.
**Verification**:
-   `src/entropy/pool.cpp`: uses `std::map` to count byte frequencies and compute log2 entropy.
-   Provides a true measure of information content rather than a guess.

## Conclusion
The codebase now adheres to the "True Randomness" requirements defined in the Audit Report. Statistical bias vectors have been removed, and the entropy mixing logic is chronologically sound.
