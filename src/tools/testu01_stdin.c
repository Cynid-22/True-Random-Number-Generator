#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unif01.h"
#include "bbattery.h"

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

static unsigned int input_buffer[4096];
static int buffer_position = 4096;
static int buffer_length = 4096;

static void refill_input_buffer(void) {
    buffer_length = (int)fread(input_buffer, sizeof(unsigned int), 4096, stdin);
    if (buffer_length <= 0) {
        fprintf(stderr, "testu01_stdin: end of input stream\n");
        exit(1);
    }
    buffer_position = 0;
}

static unsigned int read_next_stdin_word(void) {
    if (buffer_position >= buffer_length) {
        refill_input_buffer();
    }
    return input_buffer[buffer_position++];
}

int main(int argc, char *argv[]) {
    unif01_Gen *generator;
    const char *battery = "BigCrush";

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    if (argc > 1) {
        battery = argv[1];
    }

    generator = unif01_CreateExternGenBits("stdin", read_next_stdin_word);

    fprintf(stderr, "testu01_stdin: Running %s battery...\n", battery);

    if (strcmp(battery, "SmallCrush") == 0) {
        bbattery_SmallCrush(generator);
    } else if (strcmp(battery, "Crush") == 0) {
        bbattery_Crush(generator);
    } else if (strcmp(battery, "BigCrush") == 0) {
        bbattery_BigCrush(generator);
    } else {
        fprintf(stderr, "Unknown battery: %s\n", battery);
        fprintf(stderr, "Usage: %s [SmallCrush|Crush|BigCrush]\n", argv[0]);
        return 1;
    }

    unif01_DeleteExternGenBits(generator);
    return 0;
}
