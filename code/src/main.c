#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "sd_logger.h"

#define LED_1 26 // blue
#define LED_2 27 // red

static void error(void);

int main(void) {
    gpio_init(LED_1);
    gpio_set_dir(LED_1, GPIO_OUT);

    gpio_init(LED_2);
    gpio_set_dir(LED_2, GPIO_OUT);

    gpio_put(LED_1, 0);
    gpio_put(LED_2, 0);

    stdio_init_all(); // USB as stdout
    sleep_ms(3000);

    printf("Starting\n");
    stdio_flush();

    if (!sd_logger_init()) {
        printf("SD init failed\n");
        stdio_flush();
        error();
    }

    int counter = 0;

    if (!sd_logger_increment_counter(&counter)) {
        printf("Counter failed\n");
        stdio_flush();
        error();
    }

    printf("Boot counter: %d\n", counter);
    stdio_flush();

    if (!sd_logger_append_csv("time_ms,temp,pressure,accel_x\n")) {
        printf("CSV header write failed\n");
        stdio_flush();
        error();
    }

    if (!sd_logger_append_csv("1000,24.5,101325,0.01\n")) {
        printf("CSV row write failed\n");
        stdio_flush();
        error();
    }

    printf("SD logger test OK\n");
    stdio_flush();

    while (true) {
        printf("Alive. Boot counter: %d\n", counter);
        stdio_flush();

        gpio_put(LED_1, 1);
        sleep_ms(100);
        gpio_put(LED_1, 0);
        sleep_ms(900);
    }
}

//--- helpers' definition ---
static void error(void) {
    gpio_put(LED_1, 0);

    while (true) {
        gpio_put(LED_2, 1);
        sleep_ms(200);
        gpio_put(LED_2, 0);
        sleep_ms(200);
    }
}