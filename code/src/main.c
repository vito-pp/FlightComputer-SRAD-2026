#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "ff.h"

#define LED_1 26 // blue = success/progress
#define LED_2 27 // red = error

// error codes:
// 1 = mount failed
// 2 = open for write failed
// 3 = write failed
// 4 = partial write
// 5 = close after write failed
// 6 = open for read failed
// 7 = read failed
// 8 = close after read failed
// 9 = verify failed

static void error_blink(int code) {
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

    stdio_init_all();

    while (!stdio_usb_connected()) {
        gpio_put(LED_1, 1);
        sleep_ms(100);
        gpio_put(LED_1, 0);
        sleep_ms(900);
    }

    printf("USB connected\n");

    FATFS fs;
    FIL file;
    FRESULT fr;
    UINT written;
    UINT read_bytes;

    const char *msg = "hello from RP2040 SDIO\n";
    char buffer[64] = {0};

    printf("Mounting SD...\n");
    fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) {
        printf("f_mount failed: %d\n", fr);
        error_blink(1);
    }

    printf("Opening file for write...\n");
    fr = f_open(&file, "0:/test.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        printf("f_open write failed: %d\n", fr);
        error_blink(2);
    }

    printf("Writing file...\n");
    fr = f_write(&file, msg, strlen(msg), &written);
    if (fr != FR_OK) {
        printf("f_write failed: %d\n", fr);
        error_blink(3);
    }

    if (written != strlen(msg)) {
        printf("partial write: wrote %u of %u bytes\n",
               written,
               (unsigned)strlen(msg));
        error_blink(4);
    }

    fr = f_close(&file);
    if (fr != FR_OK) {
        printf("f_close after write failed: %d\n", fr);
        error_blink(5);
    }

    printf("Write complete. Bytes written: %u\n", written);

    printf("Opening file for read...\n");
    fr = f_open(&file, "0:/test.txt", FA_READ);
    if (fr != FR_OK) {
        printf("f_open read failed: %d\n", fr);
        error_blink(6);
    }

    printf("Reading file...\n");
    fr = f_read(&file, buffer, sizeof(buffer) - 1, &read_bytes);
    if (fr != FR_OK) {
        printf("f_read failed: %d\n", fr);
        error_blink(7);
    }

    buffer[read_bytes] = '\0';

    fr = f_close(&file);
    if (fr != FR_OK) {
        printf("f_close after read failed: %d\n", fr);
        error_blink(8);
    }

    printf("Read back %u bytes:\n", read_bytes);
    printf("%s\n", buffer);

    if (strcmp(buffer, msg) == 0) {
        printf("SD WRITE + READ VERIFY OK\n");

        // Success: blue LED stays on
        gpio_put(LED_1, 1);
        gpio_put(LED_2, 0);
    } else {
        printf("VERIFY FAILED\n");
        printf("Expected: %s\n", msg);
        printf("Got:      %s\n", buffer);
        error_blink(9);
    }

    f_unmount("0:");

    while (true) {
        sleep_ms(1000);
    }
}
