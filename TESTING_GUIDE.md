# Statistical Testing Guide

This guide covers how to build the CLI generator and run 4 statistical test suites against the TRNG's Quad-Layer CSPRNG pipeline.

---

## Prerequisites

Install all required MSYS2 packages first:
```bash
pacman -S git mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-jsoncpp mingw-w64-ucrt-x86_64-bzip2 mingw-w64-ucrt-x86_64-openssl
```

---

## Step 1: Build the CLI Generator

The CLI tool (`trng_gen.exe`) outputs raw binary data to `stdout`. It uses the same Quad-Layer pipeline (HKDF → ChaCha20 → XOR → AES-256 → ChaCha20) as the main application, with no GUI dependencies.

1.  Open your **MSYS2 UCRT64** terminal.
2.  Navigate to the TRNG project root:
    ```bash
    cd /c/Users/$USER/OneDrive/Desktop/Git-temp/TRNG   # adjust to your path
    ```
3.  Build:
    ```bash
    bash build_gen.sh
    ```
    This produces `trng_gen.exe` in the project root.

4.  **Quick sanity check** (dump 1 MB to a file):
    ```bash
    ./trng_gen.exe | head -c 1048576 > test.bin
    ls -la test.bin    # Should be exactly 1,048,576 bytes
    ```

---

## Step 2: Running the Test Suites

All 4 suites can run **in parallel** in separate terminal windows. Each instance of `trng_gen.exe` produces an independent stream.

**Set these variables once** (adjust to where you installed each tool):
```bash
export TRNG_DIR=~/OneDrive/Desktop/Git-temp/TRNG
export STS_DIR=~/OneDrive/Desktop/sts-2.1.2/sts-2.1.2
export PRACTRAND_DIR=~/OneDrive/Desktop/PractRand
export TESTU01_DIR=~/OneDrive/Desktop/TestU01
export SP90B_DIR=~/OneDrive/Desktop/SP800-90B_EntropyAssessment
```

---

### Test 1: NIST SP 800-22 (Statistical Test Suite)

**What it tests**: 15 statistical tests for randomness (frequency, runs, FFT, entropy, etc.)

**Download & Install**:
1. Download from NIST: https://csrc.nist.gov/projects/random-bit-generation/documentation-and-software
   - Direct link: look for **"Statistical Test Suite (STS) — sts-2.1.2"** and download the `.zip`
2. Extract the zip to your Desktop (you'll get `sts-2.1.2/sts-2.1.2/`)
3. Build in MSYS2:
```bash
cd $STS_DIR
# Fix the makefile: open it and change line 1 from "CC = /usr/bin/gcc" to "CC = gcc"
sed -i 's|CC = /usr/bin/gcc|CC = gcc|' makefile
make
```
This creates `assess.exe` in the same folder.

**Generate test data** (choose a size):
```bash
# 100 MB (800 bitstreams) — quick test
$TRNG_DIR/trng_gen.exe | head -c 104857600 > $STS_DIR/data/nist_test.bin

# 1 GB (8000 bitstreams) — thorough test
$TRNG_DIR/trng_gen.exe | head -c 1073741824 > $STS_DIR/data/nist_test.bin
```

**Run**:
```bash
cd $STS_DIR
./assess.exe 1000000
```

**Answer the prompts in this exact order**:
1.  **Generator Selection**: `0` (Input File)
2.  **User Prescribed Input File**: `data/nist_test.bin`
3.  **Select Test**: `1` (All tests)
4.  **Parameter Adjustments**: `0` (Defaults)
5.  **How many bitstreams?**: `800` (for 100 MB) or `8000` (for 1 GB)
6.  **Input File Format**: `1` (Binary)

**Check results**:
```
experiments/AlgorithmTesting/finalAnalysisReport.txt
```
A passing score is **> 96%** per test (e.g., 96/100 streams).

**Time**: ~5 min (100 MB) or ~30 min (1 GB)

---

### Test 2: PractRand (Gold Standard)

**What it tests**: The most sensitive suite available. Tests progressively from 1 KB to 1 TB+, reporting at each power-of-two. Any weakness will be caught early.

**Download & Install**:
1. Download from SourceForge: https://sourceforge.net/projects/pracrand/files/
   - Download the latest `.zip` (e.g., `PractRand-pre0.95.zip`)
2. Extract to your Desktop (you'll get a `PractRand/` folder)
3. Build in MSYS2:
```bash
cd $PRACTRAND_DIR
g++ -O3 -DWIN32 -o RNG_test tools/RNG_test.cpp $(find src -name "*.cpp") -I include -std=c++14
```
> **Important**: The `-DWIN32` flag is required on MSYS2. Without it, PractRand won't read binary stdin correctly.

This creates `RNG_test.exe` in the PractRand folder.

**Run** (direct pipe — no file needed):
```bash
$TRNG_DIR/trng_gen.exe | $PRACTRAND_DIR/RNG_test stdin
```

**Reading results**: PractRand prints results at each power-of-two size:
```
rng=stdin, seed=unknown
length= 1 megabyte (2^20 bytes), time= 0.2 seconds
  no anomalies in 6 test result(s)

length= 2 megabytes (2^21 bytes), time= 0.5 seconds
  no anomalies in 8 test result(s)

...continues doubling until failure or Ctrl+C...
```
- **"no anomalies"** = PASS at that level
- **"FAIL"** = stop, there's a problem
- Passing up to **32 GB** is excellent. **256 GB+** is exceptional.

**Time**: Seconds for first results, hours for TB-scale. You can **Ctrl+C anytime**.

---

### Test 3: TestU01 (Big Crush)

**What it tests**: 106 tests in the Big Crush battery — the most thorough academic test suite.

**Download & Install**:
1. Download TestU01 from: http://simul.iro.umontreal.ca/testu01/tu01.html
   - Click the **"TestU01"** download link (tarball `.tar.gz`)
2. Extract to your Desktop
3. Build in MSYS2:
```bash
cd $TESTU01_DIR
LIBS="-lws2_32" ./configure --prefix=/ucrt64
make
make install
```
> **Important**: The `LIBS="-lws2_32"` flag is required on MSYS2. Without it, the build fails with `undefined reference to gethostname`.
4. Install a stdin wrapper (required for piping):
```bash
# Clone the testingRNG repo which includes a stdin wrapper:
git clone https://github.com/lemire/testingRNG.git $TESTU01_DIR/testingRNG
cd $TESTU01_DIR/testingRNG/testu01
make    # Builds testu01_stdin
cp testu01_stdin $TESTU01_DIR/
```

> **Note**: TestU01 is the hardest to set up on Windows/MSYS2. If it fails, **skip it** — PractRand is equally powerful and much easier to install.

**Run** (via stdin wrapper):
```bash
$TRNG_DIR/trng_gen.exe | $TESTU01_DIR/testu01_stdin BigCrush
```

**Reading results**: Each of the 106 tests reports a p-value. A p-value outside [0.001, 0.999] is suspicious.

**Time**: Several hours (Big Crush processes ~200 GB of data)

> **Note**: TestU01 is the hardest to set up on Windows. If installation is difficult, **PractRand is equally powerful** and much easier. Prioritize PractRand over TestU01.

---

### Test 4: NIST SP 800-90B (Entropy Source Validation)

**What it tests**: Estimates the actual min-entropy of data. Unlike 800-22, this is designed to measure entropy quality, not just statistical randomness.

**Download & Install**:

Requires the prerequisite packages from above, plus `libdivsufsort` (built from source):
```bash
# Build libdivsufsort (not available via pacman):
git clone https://github.com/y-256/libdivsufsort.git /tmp/libdivsufsort
cd /tmp/libdivsufsort && mkdir build && cd build
cmake -G "MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=/ucrt64 -DBUILD_DIVSUFSORT64=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ..
make && make install
```

```bash
# Clone from GitHub:
git clone https://github.com/usnistgov/SP800-90B_EntropyAssessment.git $SP90B_DIR

# Build the C++ estimators:
cd $SP90B_DIR/cpp
make
```
This produces the executables (`ea_non_iid`, `ea_iid`, `ea_restart`, `ea_conditioning`) in the `cpp/` folder.

**Generate test data** (1 MB – 10 MB):
```bash
# 1 MB (fast, sufficient for basic estimate)
$TRNG_DIR/trng_gen.exe | head -c 1048576 > $SP90B_DIR/bin/trng_90b.bin

# 10 MB (more accurate, but slower analysis)
$TRNG_DIR/trng_gen.exe | head -c 10485760 > $SP90B_DIR/bin/trng_90b.bin
```

**Run**:
```bash
cd $SP90B_DIR/cpp
./ea_non_iid ../bin/trng_90b.bin 8
```
- `8` = bits per symbol (1 byte = 8 bits)
- Use `ea_non_iid` (non-IID) since CSPRNG output is not strictly IID

**Reading results**: The tool reports min-entropy per byte. For a good CSPRNG:
```
min-entropy = 7.xxxx bits per 8-bit symbol
```
A value close to **8.0** means the output is indistinguishable from perfect randomness.

**Time**: ~10-60 minutes depending on file size

---

## Running All 4 in Parallel

Open 4 separate MSYS2 terminals and run simultaneously:

```bash
Terminal 1 (PractRand):     $TRNG_DIR/trng_gen.exe | $PRACTRAND_DIR/RNG_test stdin
Terminal 2 (800-22 prep):   $TRNG_DIR/trng_gen.exe | head -c 1073741824 > $STS_DIR/data/nist.bin
Terminal 3 (800-90B prep):  $TRNG_DIR/trng_gen.exe | head -c 10485760 > $SP90B_DIR/data/raw_90b.bin
Terminal 4 (TestU01):       $TRNG_DIR/trng_gen.exe | $TESTU01_DIR/testu01_stdin BigCrush
```

Once Terminals 2 and 3 finish generating files, run:
```bash
Terminal 2: cd $STS_DIR && ./assess.exe 1000000
Terminal 3: cd $SP90B_DIR/cpp && ./ea_non_iid ../bin/raw_90b.bin 8
```

---

## Summary Table

| Suite | Method | Data | Time | Priority |
|-------|--------|------|------|----------|
| **PractRand** | Direct pipe | Progressive (1 KB → TB) | Minutes → hours | Highest |
| **NIST 800-22** | File dump → tool | 100 MB – 1 GB | 5-30 min | High |
| **800-90B** | File dump → C++ tool | 1-10 MB | 10-60 min | Medium |
| **TestU01** | Direct pipe (via wrapper) | ~200 GB | Hours | Low (hard to install) |
