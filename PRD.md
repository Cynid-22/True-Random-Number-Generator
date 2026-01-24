# TRNG (True Random Number Generator) - Product Requirements Document

## 1. Overview

### 1.1 Product Summary
A C++ desktop application that generates cryptographically-strong true random numbers by harvesting entropy from multiple hardware and user-interaction sources. The application features a modern GUI for user interaction and provides robust entropy estimation to ensure output quality.

### 1.2 Key Objectives
- Generate high-quality true random numbers from physical entropy sources
- Provide a user-friendly GUI for random number generation
- Implement conservative entropy estimation to guarantee sufficient randomness
- Support multiple output formats for various use cases

---

## 2. Entropy Sources

The application will collect entropy from one or more of the following sources. Users can enable/disable each source via the GUI.

### 2.1 Microphone Noise
- **Description**: Capture ambient audio noise from the system microphone
- **Implementation**: Sample raw audio data and extract LSBs (Least Significant Bits)
- **Entropy Quality**: High (environmental thermal noise)
- **Requirements**: System microphone access permission

### 2.2 Keystroke Dynamics
- **Description**: Measure timing intervals between keystrokes
- **Implementation**: Record precise timestamps of key press/release events
- **Entropy Extraction**: Inter-keystroke timing deltas (microsecond precision)
- **Requirements**: User interaction required

### 2.3 CPU/System Clock Drift
- **Description**: Exploit timing inconsistencies between system clocks
- **Implementation**: Compare high-resolution timer vs system clock over intervals
- **Entropy Quality**: Moderate (hardware-dependent)
- **Requirements**: High-resolution timer access (QueryPerformanceCounter on Windows)

### 2.4 CPU Execution Jitter
- **Description**: Measure variations in CPU instruction execution times
- **Implementation**: Execute deterministic code blocks and measure timing variance
- **Entropy Quality**: High (thermal noise, cache effects, interrupts)
- **Requirements**: None (software-only)

### 2.5 Mouse Movement
- **Description**: Capture mouse position, velocity, and timing
- **Implementation**: Record cursor coordinates and movement timestamps
- **Entropy Extraction**: Position deltas, velocity changes, timing irregularities
- **Requirements**: User interaction required

---

## 3. Entropy Estimation

### 3.1 Conservative Estimation Philosophy
> **Critical Requirement**: The entropy estimator MUST underestimate the actual entropy collected. It is better to collect 10x more data than needed than to produce weak random output.

### 3.2 Estimation Methods
- **Min-Entropy Estimation**: Use NIST SP 800-90B compliant methods
- **Compression Ratio Testing**: Estimate entropy via compression algorithms
- **Chi-Square Analysis**: Statistical uniformity testing

### 3.3 Safety Margins
- Apply a **50% safety factor** to all entropy estimates
- Example: If 256 bits of entropy are estimated, report only 128 bits as "usable"
- Display both raw and conservative estimates in the GUI

### 3.4 Visual Feedback
- **Entropy Pool Meter**: Visual progress bar showing collected entropy
- **Quality Indicator**: Color-coded status (Red → Yellow → Green)
- **Minimum Threshold**: Prevent output generation until sufficient entropy is collected

---

## 4. Graphical User Interface (GUI)

### 4.1 Technology Stack
- **Framework**: Dear ImGui (immediate-mode GUI)
- **Rendering Backend**: DirectX 11 (Windows native, no external dependencies)
- **Design**: Modern, dark-themed UI with clear visual hierarchy
- **Advantages**:
  - Lightweight, single-header library
  - No external runtime dependencies
  - Popular for developer tools and utilities
  - Easy to style and customize

### 4.2 Main Interface Components

#### 4.2.1 Entropy Collection Panel
- Checkboxes to enable/disable each entropy source
- Real-time status indicators for each active source
- "Start Collection" / "Stop Collection" buttons
- Entropy pool progress bar with bit count display

#### 4.2.2 Output Configuration Panel
- Output format selector (dropdown/radio buttons)
- Format-specific parameter inputs
- "Generate" button (disabled until sufficient entropy)
- "Copy to Clipboard" button

#### 4.2.3 Results Display
- Generated output display area
- Entropy consumption indicator
- Generation timestamp

### 4.3 User Experience Requirements
- Responsive UI during entropy collection (non-blocking)
- Clear error messages for permission issues
- Tooltips explaining each entropy source
- Keyboard shortcuts for common actions

---

## 5. Output Formats

### 5.1 Decimal Number (0 to 1)
- **Description**: A floating-point number between 0 and 1
- **Parameters**: 
  - `x` = Number of decimal digits (1-50)
- **Example**: `0.83729104628` (11 digits)

### 5.2 Integer Range
- **Description**: A random integer within a specified range
- **Parameters**:
  - `x` = Minimum value (inclusive)
  - `y` = Maximum value (inclusive)
- **Example**: Range [1, 100] → `47`

### 5.3 Binary String
- **Description**: A string consisting only of '1' and '0' characters
- **Parameters**:
  - `x` = String length (1-10000)
- **Example**: `10110010` (8 characters)

### 5.4 Custom Character String
- **Description**: A string composed of selected character sets
- **Parameters**:
  - `x` = String length
  - Character set toggles:
    - [ ] Numbers (0-9)
    - [ ] Uppercase letters (A-Z)
    - [ ] Lowercase letters (a-z)
    - [ ] Special characters (!@#$%^&*()_+-=[]{}|;':\",./<>?)
- **Example**: With all sets enabled, length 16 → `aB3$kL9@mN2#pQ5!`

### 5.5 Automatic Bit/Byte Output
- **Description**: Generate random data of specified size
- **Parameters**:
  - `x` = Amount
  - Unit = Bits or Bytes
- **Output Options**:
  - Hexadecimal string
  - Base64 encoded string
  - Raw binary file export

---

## 6. Technical Architecture

### 6.1 Core Components

```
┌─────────────────────────────────────────────────────────────┐
│              Dear ImGui GUI Layer (DirectX 11)              │
├─────────────────────────────────────────────────────────────┤
│                   Output Formatter                          │
├─────────────────────────────────────────────────────────────┤
│              Entropy Pool Manager                           │
├───────────┬───────────┬───────────┬───────────┬────────────┤
│ Microphone│ Keystroke │   Clock   │   CPU     │   Mouse    │
│  Sampler  │  Monitor  │   Drift   │  Jitter   │  Tracker   │
├───────────┴───────────┴───────────┴───────────┴────────────┤
│                  Entropy Estimator                          │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 Module Descriptions

| Module | Responsibility |
|--------|----------------|
| **EntropySource** | Abstract base class for all entropy sources |
| **EntropyPool** | Accumulates and mixes raw entropy |
| **EntropyEstimator** | Conservative entropy estimation |
| **OutputFormatter** | Converts raw bits to requested format |
| **Application** | Dear ImGui + DirectX 11 GUI controller |

### 6.3 Thread Model
- **Main Thread**: GUI event loop
- **Collector Threads**: One per active entropy source
- **Thread-Safe Pool**: Mutex-protected entropy accumulation

---

## 7. Security Considerations

### 7.1 Entropy Mixing
- Use cryptographic hash (SHA-256) to mix entropy from multiple sources
- Never output raw entropy directly

### 7.2 Memory Protection
- Zero-out entropy buffers after use
- Avoid swapping sensitive memory to disk (mlock where available)

### 7.3 Output Bias Prevention
- Use rejection sampling for range-based outputs to avoid modulo bias
- Validate output uniformity during testing

---

## 8. Build & Dependencies

### 8.1 Required Dependencies
- **Dear ImGui**: Immediate-mode GUI library (included in project)
- **DirectX 11**: Rendering backend (Windows SDK, comes with Visual Studio)
- **Windows Audio API (WASAPI)**: Microphone access (Windows native)
- **No external runtime dependencies** — everything compiles into a single executable

### 8.2 Build System
- **Build Script**: Simple `build.bat` batch file (no CMake required)
- **Compiler**: MinGW-w64 (GCC for Windows, via MSYS2)
- **Build Command**: Run `build.bat` from Command Prompt

### 8.3 Platform Support
- **Primary**: Windows 10/11
- **Future**: macOS, Linux (ImGui supports OpenGL backend for cross-platform)

---

## 9. Testing Requirements

### 9.1 Unit Tests
- Test each entropy source in isolation
- Verify entropy estimator underestimates (via known inputs)
- Validate output format correctness

### 9.2 Statistical Tests
- Run NIST SP 800-22 test suite on outputs
- Verify chi-square distribution compliance
- Test for serial correlation

### 9.3 UI Tests
- Verify all controls function correctly
- Test edge cases (min/max parameters)
- Stress test with rapid generation requests

---

## 10. Future Enhancements (Out of Scope for v1.0)

- [ ] Hardware RNG support (RDRAND instruction)
- [ ] Network camera video noise as entropy
- [ ] Export entropy pool state for resume
- [ ] REST API for headless operation
- [ ] Entropy source quality comparison dashboard

---

## 11. Success Criteria

1. ✅ All 5 entropy sources implemented and functional
2. ✅ Conservative entropy estimation with 50% safety margin
3. ✅ All 5 output formats working correctly
4. ✅ GUI is responsive during entropy collection
5. ✅ Passes NIST randomness tests with >99% of test cases

---

## Appendix A: Entropy Calculations

### Bits Required per Output Type

| Output Type | Formula |
|-------------|---------|
| Decimal 0-1 (x digits) | `x * log2(10) ≈ x * 3.322 bits` |
| Integer [x, y] | `ceil(log2(y - x + 1)) bits` + rejection overhead |
| Binary string (x chars) | `x bits` |
| Custom string (x chars, n charset size) | `x * log2(n) bits` |

### Safety Buffer
Always collect **at least 2x** the calculated minimum before enabling generation.

---

*Document Version: 1.1*  
*Created: 2026-01-23*  
*Last Updated: 2026-01-23*  
*Change Log: v1.1 - Switched from Qt 6 to Dear ImGui with DirectX 11 backend*
