#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "ff.h"

#define LED_1 26 // blue
#define LED_2 27 // red

static void error_blink(int code) {
    gpio_put(LED_1, 0);

    while (true) {
        for (int i = 0; i < code; i++) {
            gpio_put(LED_2, 1);
            sleep_ms(150);
            gpio_put(LED_2, 0);
            sleep_ms(150);
        }
        sleep_ms(1000);
    }
}

int main() {
    gpio_init(LED_1);
    gpio_set_dir(LED_1, GPIO_OUT);

    gpio_init(LED_2);
    gpio_set_dir(LED_2, GPIO_OUT);

    gpio_put(LED_1, 0);
    gpio_put(LED_2, 0);

    stdio_init_all();
    sleep_ms(3000);

    for (int i = 0; i < 5; i++) {
        gpio_put(LED_1, 1);
        printf("BOOT %d\n", i);
        stdio_flush();
        sleep_ms(200);

        gpio_put(LED_1, 0);
        sleep_ms(200);
    }

    printf("Starting SD persistent counter test...\n");
    stdio_flush();

    FATFS fs;
    FIL file;
    FRESULT fr;
    UINT br = 0;
    UINT bw = 0;

    char buffer[64] = {0};
    int counter = 0;

    printf("Before f_mount\n");
    stdio_flush();

    gpio_put(LED_1, 1); // blue ON = inside f_mount
    fr = f_mount(&fs, "0:", 1);
    gpio_put(LED_1, 0); // blue OFF = f_mount returned

    printf("After f_mount: %d\n", fr);
    stdio_flush();

    if (fr != FR_OK) {
        error_blink(1);
    }

    printf("SD mounted OK\n");
    stdio_flush();

    printf("Opening counter.txt for read...\n");
    stdio_flush();

    fr = f_open(&file, "0:/counter.txt", FA_READ);

    if (fr == FR_OK) {
        printf("counter.txt found\n");
        stdio_flush();

        fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
        f_close(&file);

        if (fr != FR_OK) {
            printf("f_read failed: %d\n", fr);
            stdio_flush();
            error_blink(2);
        }

        buffer[br] = '\0';
        counter = atoi(buffer);

        printf("Previous counter value: %d\n", counter);
        stdio_flush();
    } else {
        printf("counter.txt not found or open failed: %d\n", fr);
        printf("Starting at 0.\n");
        stdio_flush();
        counter = 0;
    }

    counter++;

    printf("New counter value: %d\n", counter);
    stdio_flush();

    char out[64];
    snprintf(out, sizeof(out), "%d\n", counter);

    printf("Opening counter.txt for write...\n");
    stdio_flush();

    fr = f_open(&file, "0:/counter.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("f_open write failed: %d\n", fr);
        stdio_flush();
        error_blink(3);
    }

    printf("Writing counter...\n");
    stdio_flush();

    fr = f_write(&file, out, strlen(out), &bw);
    if (fr != FR_OK || bw != strlen(out)) {
        printf("f_write failed: %d, bytes written: %u\n", fr, bw);
        stdio_flush();
        error_blink(4);
    }

    printf("Syncing file...\n");
    stdio_flush();

    fr = f_sync(&file);
    if (fr != FR_OK) {
        printf("f_sync failed: %d\n", fr);
        stdio_flush();
        error_blink(5);
    }

    printf("Closing file...\n");
    stdio_flush();

    fr = f_close(&file);
    if (fr != FR_OK) {
        printf("f_close failed: %d\n", fr);
        stdio_flush();
        error_blink(6);
    }

    printf("Write complete. Bytes written: %u\n", bw);
    stdio_flush();

    memset(buffer, 0, sizeof(buffer));
    br = 0;

    printf("Opening counter.txt for verify read...\n");
    stdio_flush();

    fr = f_open(&file, "0:/counter.txt", FA_READ);
    if (fr != FR_OK) {
        printf("f_open verify failed: %d\n", fr);
        stdio_flush();
        error_blink(7);
    }

    printf("Reading back counter...\n");
    stdio_flush();

    fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
    f_close(&file);

    if (fr != FR_OK) {
        printf("f_read verify failed: %d\n", fr);
        stdio_flush();
        error_blink(8);
    }

    buffer[br] = '\0';

    printf("Read-back value: %s", buffer);
    stdio_flush();

    if (atoi(buffer) == counter) {
        printf("PERSISTENT COUNTER VERIFY OK\n");
        stdio_flush();

        gpio_put(LED_1, 1);
        gpio_put(LED_2, 0);
    } else {
        printf("VERIFY FAILED\n");
        printf("Expected: %d\n", counter);
        printf("Got raw: %s\n", buffer);
        stdio_flush();
        error_blink(9);
    }

    f_unmount("0:");

    while (true) {
        printf("Alive. Counter stored as: %d\n", counter);
        stdio_flush();

        gpio_put(LED_1, 1);
        sleep_ms(100);
        gpio_put(LED_1, 0);
        sleep_ms(1900);
    }
}
