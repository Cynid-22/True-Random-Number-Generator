#!/bin/bash
# Build the standalone TRNG generator for piping to test suites
# Usage: ./build_gen.sh

set -e

echo "Building trng_gen.exe..."

g++ -O3 -o trng_gen.exe \
  src/tools/trng_gen.cpp \
  src/crypto/sha512.cpp \
  src/crypto/hkdf.cpp \
  src/crypto/chacha20.cpp \
  src/crypto/aes.cpp \
  -I src -std=c++17

echo "Done! Built trng_gen.exe"
echo ""
echo "Quick test:  ./trng_gen.exe | head -c 1M > test.bin"
echo "PractRand:   ./trng_gen.exe | ./RNG_test stdin"
echo "Dieharder:   ./trng_gen.exe | dieharder -a -g 200"
