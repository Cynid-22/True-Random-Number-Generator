# Statistical Validation Results

> **These results are from the earlier quad-layer pipeline (ChaCha20, AES-256), which HMAC_DRBG replaced. They have not been rerun for HMAC_DRBG yet.** The steps to rerun them are in [TESTING_GUIDE.md](../TESTING_GUIDE.md).

All tests ran against the pipeline output of the CLI tool `trng_gen.exe`. They show that the output has no detectable statistical bias. They say nothing about how much entropy the sources collect, since a CSPRNG's output passes them whatever its seed. For the sources, see [Measured Source Entropy](../README.md#measured-source-entropy-nist-sp-800-90b).

## Summary

| Suite | Result | Key metric |
|-------|--------|------------|
| **NIST SP 800-22** | PASS | 8000 streams (1 GB), ~99% pass rate across all tests |
| **PractRand** | PASS | 2 TB, 190 tests, no persistent anomalies |
| **NIST SP 800-90B** | PASS | 7.37 - 7.53 bits per 8-bit symbol (non-IID) |
| **TestU01** | PASS | SmallCrush 15/15, Crush 143/144, BigCrush 160/160 statistics |

---

## NIST SP 800-22 (Statistical Test Suite)

**Parameters**: 1,000,000-bit streams × 8000 streams | 1 GB input | `assess.exe` (sts-2.1.2)

| Test | Uniformity p-value | Proportion | Passed/Total | Status |
|------|--------------------|------------|--------------|--------|
| Frequency | 0.4692 | 0.9906 | 7925/8000 | Passed |
| BlockFrequency | 0.4919 | 0.9905 | 7924/8000 | Passed |
| CumulativeSums | 0.8751 | 0.9906 | 15849/16000 | Passed |
| Runs | 0.3157 | 0.9888 | 7910/8000 | Passed |
| LongestRun | 0.9297 | 0.9892 | 7914/8000 | Passed |
| Rank | 0.3003 | 0.9896 | 7917/8000 | Passed |
| FFT | 0.0008 | 0.9858 | 7886/8000 | Passed* (margin) |
| NonOverlappingTemplate | 0.0803 | 0.9898 | 1171933/1184000 | Passed |
| OverlappingTemplate | 0.4238 | 0.9889 | 7911/8000 | Passed |
| Universal | 0.0878 | 0.9865 | 7892/8000 | Passed* (margin) |
| ApproximateEntropy | 0.0760 | 0.9895 | 7916/8000 | Passed |
| RandomExcursions | 0.6620 | 0.9904 | 38150/38519 | Passed |
| RandomExcursionsVariant | 0.1510 | 0.9904 | 85839/86670 | Passed |
| Serial | 0.8639 | 0.9902 | 15844/16000 | Passed |
| LinearComplexity | 0.1585 | 0.9905 | 7924/8000 | Passed |

**Result**: PASS. Every uniformity p-value is at least 0.0001.

- **FFT and Universal**: for 8000 streams the NIST lower bound on the proportion is 7893 passes (0.9867). FFT passed 7886 streams (7 short) and Universal 7892 (1 short). Over 15 tests on 1 GB, a proportion this close to the bound is expected now and then by chance.
- **RandomExcursions**: these two tests skip streams that have too few cycles. With those streams left out, both pass on more than 99.04% of the rest.

---

## PractRand (v0.96)

**Parameters**: direct pipe from `trng_gen.exe` | `stdin8` | progressive testing

| Data processed | Tests run | Time | Result |
|---------------|-----------|------|--------|
| 512 MB (2^29) | 118 | 3.5s | no anomalies |
| 1 GB (2^30) | 125 | 6.9s | no anomalies |
| 2 GB (2^31) | 132 | 13.7s | no anomalies |
| 4 GB (2^32) | 139 | 28.4s | no anomalies |
| 8 GB (2^33) | 145 | 54.5s | no anomalies |
| 16 GB (2^34) | 151 | 107s | no anomalies |
| 32 GB (2^35) | 157 | 214s | no anomalies |
| 64 GB (2^36) | 163 | 421s | no anomalies |
| 128 GB (2^37) | 169 | 821s | no anomalies |
| 256 GB (2^38) | 175 | 1629s | 1 mildly suspicious (noise) |
| 512 GB (2^39) | 180 | 3251s | no anomalies |
| 1 TB (2^40) | 185 | 7097s | no anomalies |
| 2 TB (2^41) | 190 | 13269s | no anomalies |

**Result**: PASS at 2 TB with 190 tests. One result at 256 GB was flagged as mildly suspicious (p = 4e-5). It was gone at 512 GB and stayed clear through 2 TB, so it was chance and not a pattern in the output.

---

## NIST SP 800-90B (Entropy Assessment)

**Parameters**: 1 MB, 10 MB and 100 MB input files | `ea_non_iid -a` | 8 bits per symbol

SP 800-90B is meant for raw entropy-source data. Run on CSPRNG output, it only checks that the output shows no structure its estimators can find; it does not measure the entropy behind it. The measurements of the raw sources are in [Measured Source Entropy](../README.md#measured-source-entropy-nist-sp-800-90b).

### Non-IID test

| Run | Input size | H_original | H_bitstring | Final estimate |
|-----|-----------|-----------|-------------|----------------|
| 1 | 1 MB | 7.368524 | 0.924979 | **7.368524** |
| 2 | 10 MB | 7.487618 | 0.944990 | **7.487618** |
| 3 | 100 MB | 7.923559 | 0.936741 | **7.493927** |
| 4 | 100 MB | 7.926010 | 0.939662 | **7.517294** |
| 5 | 100 MB | 7.920303 | 0.941182 | **7.529456** |

Final estimate = min(H_original, 8 × H_bitstring). Each run used a new stream, so the numbers differ slightly.

### IID test (informational)

| Metric | Value | Maximum | Percentage |
|--------|-------|---------|------------|
| H_original | 7.967199 | 8.0 | 99.6% |
| H_bitstring | 0.999465 | 1.0 | 99.9% |
| Chi square | — | — | PASSED |
| Longest repeated substring | — | — | FAILED |

**Result**: PASS, 7.37 - 7.53 bits per 8-bit symbol (non-IID). The IID test is informational only; the non-IID estimate is the one to use.

---

## TestU01

**Parameters**: direct pipe from `trng_gen.exe` | `testu01_stdin` wrapper

Each battery runs a number of tests, and some tests report more than one statistic. A statistic fails when its p-value is outside `[0.001, 0.999]`.

| Battery | Tests | Statistics | CPU time | Result |
|---------|-------|------------|----------|--------|
| **SmallCrush** | 10 | 15 | ~6s | **PASS** (15/15) |
| **Crush** | 96 | 144 | ~52m | **PASS** (143/144) |
| **BigCrush** | 106 | 160 | ~4h 39m | **PASS** (160/160) |

### SmallCrush

All 15 statistics are inside the range.

| Test | Statistic | p-value | Status |
|------|-----------|---------|--------|
| `smarsa_BirthdaySpacings` | Poisson | 0.81 | Passed |
| `sknuth_Collision` | Chi-square | 0.29 | Passed |
| `sknuth_Gap` | Chi-square | 0.0082 | Passed |
| `sknuth_SimpPoker` | Chi-square | 0.93 | Passed |
| `sknuth_CouponCollector` | Chi-square | 0.32 | Passed |
| `sknuth_MaxOft` | Chi-square<br>Anderson-Darling | 0.89<br>0.19 | Passed |
| `svaria_WeightDistrib` | Chi-square | 0.24 | Passed |
| `smarsa_MatrixRank` | Chi-square | 0.01 | Passed |
| `sstring_HammingIndep` | Chi-square | 0.81 | Passed |
| `swalk_RandomWalk1` | H (Chi-square)<br>M (Chi-square)<br>J (Chi-square)<br>R (Chi-square)<br>C (Chi-square) | 0.81<br>0.80<br>0.85<br>0.02<br>0.48 | Passed |

### Crush

| Statistics | Inside the range | Status |
|------------|------------------|--------|
| All except one | 143/143 | Passed |
| `snpair_ClosePairs (t=7)` | 0/1 | Margin (0.9991) |

**Result**: PASS (143/144). The `ClosePairs` statistic had a p-value of 0.9991, just outside the 0.999 bound. With 144 statistics, about one value this close to a bound is expected by chance, so a single one does not point to a flaw.

### BigCrush

**Result**: PASS (160/160). Every statistic is inside the range.
