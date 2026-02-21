/*
 * testu01_stdin.c — Stdin wrapper for TestU01
 *
 * Reads 32-bit unsigned integers from stdin and feeds them into
 * TestU01's SmallCrush, Crush, or BigCrush batteries.
 *
 * Usage:
 *   ./trng_gen.exe | ./testu01_stdin BigCrush
 *   ./trng_gen.exe | ./testu01_stdin Crush
 *   ./trng_gen.exe | ./testu01_stdin SmallCrush
 *
 * Build (after TestU01 is installed to /ucrt64):
 *   gcc -std=c99 -O2 -o testu01_stdin testu01_stdin.c \
 *       -I/ucrt64/include -L/ucrt64/lib \
 *       -ltestu01 -lprobdist -lmylib -lm -lws2_32
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unif01.h"
#include "bbattery.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

/* Buffer for reading from stdin */
static unsigned int buffer[4096];
static int buf_pos = 4096;
static int buf_len = 4096;

static void refill_buffer(void) {
    buf_len = (int)fread(buffer, sizeof(unsigned int), 4096, stdin);
    if (buf_len <= 0) {
        fprintf(stderr, "testu01_stdin: end of input stream\n");
        exit(1);
    }
    buf_pos = 0;
}

static unsigned int stdin_Bits(void) {
    if (buf_pos >= buf_len) {
        refill_buffer();
    }
    return buffer[buf_pos++];
}

int main(int argc, char *argv[]) {
    unif01_Gen *gen;
    const char *battery = "BigCrush";

    /* Critical: set stdin to binary mode on Windows.
       Without this, 0x1A (Ctrl+Z) in random data is interpreted as EOF. */
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    if (argc > 1) {
        battery = argv[1];
    }

    /* Create a TestU01 external generator from our stdin reader */
    gen = unif01_CreateExternGenBits("stdin", stdin_Bits);

    fprintf(stderr, "testu01_stdin: Running %s battery...\n", battery);

    if (strcmp(battery, "SmallCrush") == 0) {
        bbattery_SmallCrush(gen);
    } else if (strcmp(battery, "Crush") == 0) {
        bbattery_Crush(gen);
    } else if (strcmp(battery, "BigCrush") == 0) {
        bbattery_BigCrush(gen);
    } else {
        fprintf(stderr, "Unknown battery: %s\n", battery);
        fprintf(stderr, "Usage: %s [SmallCrush|Crush|BigCrush]\n", argv[0]);
        return 1;
    }

    unif01_DeleteExternGenBits(gen);
    return 0;
}
