#!/bin/bash

set -euo pipefail

echo "========================================"
echo "TRNG Regression Tests (MinGW)"
echo "========================================"

if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ not found"
    exit 1
fi

mkdir -p build

echo "Compiling resources..."
windres src/resource.rc -O coff -o build/resource_tests.res || exit 1

echo "Compiling tests..."
g++ -std=c++17 -O2 \
    -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
    -D_WIN32_WINNT=0x0A00 \
    -I"src" -I"src/core" -I"src/logic" -I"src/platform" -I"src/entropy" \
    -I"src/entropy/clock_drift" -I"src/entropy/cpu_jitter" -I"src/entropy/microphone" -I"src/crypto" \
    -o build/TRNG_tests.exe \
    tests/test_main.cpp \
    tests/crypto_tests.cpp \
    tests/entropy_pool_tests.cpp \
    tests/generation_tests.cpp \
    tests/format_tests.cpp \
    tests/export_tests.cpp \
    src/logic/csprng.cpp \
    src/logic/output_generator.cpp \
    src/logic/output_export.cpp \
    src/logic/word_lists.cpp \
    src/logic/formats/number_formats.cpp \
    src/logic/formats/text_formats.cpp \
    src/logic/formats/raw_data_format.cpp \
    src/logic/formats/uuid_formats.cpp \
    src/logic/formats/one_time_pad_format.cpp \
    src/platform/unicode_path.cpp \
    src/platform/virtual_machine.cpp \
    src/platform/os_random.cpp \
    src/entropy/clock_drift/clock_drift.cpp \
    src/entropy/cpu_jitter/cpu_jitter.cpp \
    src/entropy/raw_input_thread.cpp \
    src/entropy/keystroke/keystroke.cpp \
    src/entropy/mouse/mouse.cpp \
    src/entropy/microphone/microphone.cpp \
    src/entropy/pool.cpp \
    src/entropy/source_credit.cpp \
    src/crypto/sha512.cpp \
    src/crypto/hmac_drbg.cpp \
    src/crypto/toeplitz.cpp \
    src/crypto/self_test.cpp \
    build/resource_tests.res \
    -static \
    -ldxgi -lshcore -luser32 -lgdi32 -limm32 -lcomdlg32 -lole32 -loleaut32 -luuid -lmmdevapi -lksuser || exit 1

echo "Running tests..."
./build/TRNG_tests.exe
