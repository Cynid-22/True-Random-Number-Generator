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

### Generation Modes

| Collected Entropy | Mode | Security Level |
|-------------------|------|----------------|
| < 512 bits | Blocked | Generation disabled |
| >= 512, < target | PSEUDO-RANDOM | Computational (CSPRNG expansion) |
| >= target | TRUE RANDOMNESS | Information Theoretic (hash consolidation) |

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
