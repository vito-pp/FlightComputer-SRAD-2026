#include <stdio.h>
#include "pico/stdlib.h"
#include "iim20670.h"
#include "buzzer.h"
#include "ms5611.h"

// GPIOs
#define BLUE_LED 26  
#define RED_LED 27 
#define I2C_PORT i2c1
#define I2C_SDA_PIN 2
#define I2C_SCL_PIN 3

#define USE_IIM_CALIBRATION 1

void error_handler(void);

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    ms5611_t baro;

    ms5611_status_t st = ms5611_init(&baro, I2C_PORT, 0x76, MS5611_OSR_4096);
    if (st != MS5611_OK) {
        printf("MS5611 init failed: %d\n", st);
        while (true) {
            sleep_ms(1000);
        }
    }

    printf("MS5611 init OK\n");

    printf("Calibrating barometer...\n");
    ms5611_calibrate(&baro, 200);
    printf("Calibration done\n");

    while (true) {
        st = ms5611_poll(&baro);

        if (st == MS5611_OK && ms5611_sample_ready(&baro)) {
            float p = ms5611_get_pressure_mbar(&baro);
            float t = ms5611_get_temperature_c(&baro);
            float alt = ms5611_get_altitude_m(&baro);

            printf("P: %.2f mbar | T: %.2f C | Alt: %.2f m\n", p, t, alt);

            ms5611_clear_sample_ready(&baro);
        }

        tight_loop_contents();
    }
}

void error_handler(void) {
	gpio_put(BLUE_LED, 0);
	gpio_put(RED_LED, 1);
	while (true) { sleep_ms(1000); }
}
