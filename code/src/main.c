#include <stdio.h>
#include "pico/stdlib.h"
#include "iim20670.h"
#include "buzzer.h"
#include "ms5611.h"

// GPIOs
#define BLUE_LED 26
#define RED_LED 27

#define USE_IIM_CALIBRATION 1

void error_handler(void);

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    if (!ms5611_init()) {
        printf("MS5611 init failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    printf("MS5611 init OK\n");

    printf("Calibrating barometer...\n");
    if (!ms5611_calibrate()) {
        printf("MS5611 calibration failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }
    printf("Calibration done\n");

    while (true) {
        ms5611_sample_t baro;
        ms5611_poll_result_t result = ms5611_poll_sample(&baro);

        if (result == MS5611_POLL_OK) {
            printf("P: %.2f mbar | T: %.2f C | Alt: %.2f m\n",
                   baro.pressure_mbar,
                   baro.temperature_c,
                   baro.altitude_m);
        }

        tight_loop_contents();
    }
}

void error_handler(void) {
	gpio_put(BLUE_LED, 0);
	gpio_put(RED_LED, 1);
	while (true) { sleep_ms(1000); }
}
