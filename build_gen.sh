#!/bin/bash

set -e

echo "Building trng_gen.exe..."

g++ -O3 -o trng_gen.exe \
  src/tools/trng_gen.cpp \
  src/crypto/sha512.cpp \
  src/crypto/hmac_drbg.cpp \
  src/platform/os_random.cpp \
  -I src -std=c++17 -lpthread

echo "Done! Built trng_gen.exe"
echo "Test stream only: do not use its output for keys, passwords or other secrets."
echo ""
echo "Quick test:  ./trng_gen.exe | head -c 1M > test.bin"
echo "PractRand:   ./trng_gen.exe | ./RNG_test stdin"
echo "Dieharder:   ./trng_gen.exe | dieharder -a -g 200"
