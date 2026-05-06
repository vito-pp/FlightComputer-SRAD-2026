#include "sd_logger.h"
#include "pico/stdio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ff.h"

static FATFS fs;
static bool mounted = false;

bool sd_logger_init(void) {
    FRESULT fr = f_mount(&fs, "0:", 1);

    if (fr != FR_OK) {
        printf("SD mount failed: %d\n", fr);
	stdio_flush();
        return false;
    }

    mounted = true;
    printf("SD mounted OK\n");
    stdio_flush();
    return true;
}

bool sd_logger_append_csv(const char *line) {
    if (!mounted) return false;

    FIL file;
    FRESULT fr;
    UINT bw;

    fr = f_open(&file, "0:/log.csv", FA_WRITE | FA_OPEN_APPEND);
    if (fr != FR_OK) {
        printf("CSV open failed: %d\n", fr);
	stdio_flush();
        return false;
    }

    fr = f_write(&file, line, strlen(line), &bw);
    if (fr != FR_OK || bw != strlen(line)) {
        printf("CSV write failed: %d\n", fr);
	stdio_flush();
        f_close(&file);
        return false;
    }

    f_sync(&file);
    f_close(&file);

    return true;
}

bool sd_logger_increment_counter(int *new_value) {
    if (!mounted) return false;

    FIL file;
    FRESULT fr;
    UINT br, bw;

    char buffer[64] = {0};
    int counter = 0;

    fr = f_open(&file, "0:/counter.txt", FA_READ);
    if (fr == FR_OK) {
        fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
        f_close(&file);

        if (fr != FR_OK) return false;

        buffer[br] = '\0';
        counter = atoi(buffer);
    }

    counter++;

    char out[64];
    snprintf(out, sizeof(out), "%d\n", counter);

    fr = f_open(&file, "0:/counter.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return false;

    fr = f_write(&file, out, strlen(out), &bw);
    if (fr != FR_OK || bw != strlen(out)) {
        f_close(&file);
        return false;
    }

    f_sync(&file);
    f_close(&file);

    if (new_value) {
        *new_value = counter;
    }

    return true;
}
