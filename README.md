# True Random Number Generator

A desktop application for generating cryptographically secure random numbers using multiple hardware entropy sources. Built with C++ and Dear ImGui for Windows.

---

## Overview

TRNG collects entropy from physical and system sources to generate high-quality random output. Unlike pseudo-random number generators that rely on deterministic algorithms, TRNG harvests genuine randomness from hardware imperfections, user behavior, and operating system chaos.

The application supports two security modes:
- **TRUE RANDOMNESS (Consolidation)**: When collected entropy exceeds the output requirement, raw data is condensed using cryptographic hashing. This provides Information Theoretic Security.
- **PSEUDO-RANDOM (Expansion)**: When collected entropy is less than required, a CSPRNG (ChaCha20) is seeded via HKDF to expand the available entropy. This provides Computational Security.

---

## Entropy Sources

### User Input Sources

| Source | Method | Data Collected |
|--------|--------|----------------|
| **Microphone** | Thermal Noise | Least Significant Bit (LSB) of 16-bit audio samples at 44.1kHz |
| **Keystroke** | Timing Analysis | Flight Time (gap between keys) and Dwell Time (key hold duration) in nanoseconds |
| **Mouse** | Movement Tracking | X/Y coordinates with nanosecond timestamps, filtered for sensor drift |

### System Input Sources

| Source | Method | Data Collected |
|--------|--------|----------------|
| **Clock Drift** | Hardware Entropy | CPU cycle count delta during fixed OS timer windows; variations caused by thermal noise |
| **CPU Jitter** | Race Condition | Counter value from parallel thread races; influenced by OS scheduling and cache contention |

---

## Output Formats

- **Decimal Number** (0.0 - 1.0)
- **Integer Range** (custom min/max)
- **Binary String**
- **Custom String** (configurable character set)
- **Bit/Byte Output** (hex, base64, binary)
- **Passphrase** (123,565 word dictionary, ~16.5 bits/word)
- **One-Time Pad** (text or file encryption)

---

## Building

### Requirements

- Windows 10/11
- MSYS2 with MinGW-w64
- g++ with C++17 support

### Build Steps

```bash
# Clone the repository
git clone https://github.com/yourusername/TRNG.git
cd TRNG

# Build (downloads ImGui automatically on first run)
./build.sh

# Run
./build/TRNG.exe
```

---

## Project Structure

```
TRNG/
├── src/
│   ├── main.cpp              # Application entry point
│   ├── core/
│   │   └── app_state.h       # Global application state
│   ├── gui/
│   │   ├── gui.h             # GUI declarations
│   │   ├── gui.cpp           # Style, menu, entropy bar
│   │   ├── gui_sources.cpp   # Input tabs, Collection Window
│   │   └── gui_output.cpp    # Output configuration
│   ├── logic/
│   │   ├── logic.h           # Logic declarations
│   │   └── logic.cpp         # Entropy calculation
│   └── platform/
│       ├── dx11.h            # DirectX declarations
│       └── dx11.cpp          # DirectX implementation
├── assets/
│   └── default_wordlist.txt  # Passphrase dictionary
├── external/
│   └── imgui/                # Dear ImGui (auto-downloaded)
└── build.sh                  # Build script
```

---

## Security Considerations

### Minimum Entropy Requirement

The application enforces a **512-bit minimum** for all output generation, regardless of the selected format. This ensures a baseline level of security even for small outputs.

### Cryptographic Architecture: Quad-Layer "National Security Grade"

All output generation uses a sophisticated four-layer pipeline designed to exceed NIST and NSA security recommendations:

1.  **Layer 1 (Masking)**: `ChaCha20` (RFC 8439) stream seeded by a `SHA-512` hash of the pool + context.
2.  **Layer 2 (Injection)**: **XOR Information Fold**. The raw entropy pool is XORed directly into the stream, ensuring Information Theoretic security (One-Time Pad property) when Input bits >= Output bits.
3.  **Layer 3 (Transformation)**: `AES-256` (FIPS 197) in CTR mode, using a dynamic key derived from the stream itself. This adds robust non-linearity.
4.  **Layer 4 (Whitening)**: Final `ChaCha20` whitening pass to remove any potential structural artifacts.

### Generation Modes

| Collected Entropy | Mode | Security Level |
|-------------------|------|----------------|
| < 512 bits | Blocked | Generation disabled |
| >= 512, < target | PSEUDO-RANDOM | Computational (3-Primitive Hardened CSPRNG) |
| >= target | TRUE RANDOMNESS | Information Theoretic (Math-guaranteed via Layer 2 XOR) |

## Documentation & Standards

| Component | Standard | Authority |
|-----------|----------|-----------|
| Hashing | FIPS 180-4 | NIST |
| Encryption| FIPS 197 | NIST |
| Stream | RFC 8439 | IETF |
| KDF | RFC 5869 | IETF |

### NIST SP 800-22 Statistical Test Suite — Validation Results

The CSPRNG output was tested against the official **NIST SP 800-22 rev1a** Statistical Test Suite (`sts-2.1.2`). This is the US government's standard benchmark for certifying random number generators.

**Test Parameters:**
- **Stream Length**: 1,000,000 bits
- **Number of Streams**: 100
- **Input**: 100 MB of raw binary data generated via Expansion Mode (Quad-Layer pipeline)

**Results:**

| Metric | Value |
|--------|-------|
| Total test instances | ~188 |
| Passed | 186 |
| Marginal (flagged `*`) | 2 |
| Failed | 0 |
| **Verdict** | **PASS** |

All 15 statistical tests passed:

| Test | P-Value | Proportion | Status |
|------|---------|------------|--------|
| Frequency | 0.554420 | 99/100 | Passed |
| Block Frequency | 0.554420 | 96/100 | Passed |
| Cumulative Sums | 0.699313 | 98/100 | Passed |
| Runs | 0.115387 | 100/100 | Passed |
| Longest Run of Ones | 0.494392 | 98/100 | Passed |
| Rank | 0.437274 | 100/100 | Passed |
| FFT | 0.657933 | 98/100 | Passed |
| Non-Overlapping Template | 0.002374 – 0.999438 | 95–100/100 | Passed |
| Overlapping Template | 0.637119 | 99/100 | Passed |
| Universal | 0.066882 | 95/100 | Passed* |
| Approximate Entropy | 0.897763 | 100/100 | Passed |
| Random Excursions | 0.213309 – 0.911413 | 61–62/62 | Passed |
| Random Excursions Variant | 0.054199 – 0.671779 | 60–62/62 | Passed |
| Serial | 0.304126 | 99/100 | Passed |
| Linear Complexity | 0.637119 | 98/100 | Passed |

> **Note**: The 2 marginal results (95/100 on one NonOverlappingTemplate and Universal) are within expected statistical noise. Even a perfect random source will produce ~1-2 borderline results out of 188 test instances by chance (1.06% ≈ expected 1%).

### Timestamp Strategy

All entropy events are timestamped with 64-bit nanosecond precision:
- Full timestamp for chronological sorting
- Last 6-9 digits (micro/nanoseconds) contribute additional hardware jitter entropy

---

## Usage

1. **Configure Sources**: Select which entropy sources to include in the User Input and System Input tabs
2. **Start Collection**: Click "Start Collection" to begin gathering entropy
3. **Monitor Progress**: Watch the entropy bar fill; security mode indicator shows consolidation vs. expansion
4. **Configure Output**: Select output format and parameters in the Output tab
5. **Generate**: Click Generate when sufficient entropy is collected

---

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui) - Immediate mode GUI (auto-downloaded)
- DirectX 11 - Windows graphics API
- Windows API - Audio capture, input hooks, high-resolution timers

---

## License

MIT License

---

## Acknowledgments

- The cryptographic community for research on hardware entropy sources
- Dear ImGui for the excellent GUI framework
