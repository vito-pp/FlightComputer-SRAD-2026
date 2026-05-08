#include <stdio.h>
#include "pico/stdlib.h"
#include "iim20670.h"

// GPIOs
#define BLUE_LED 26  
#define RED_LED 27 

#define USE_IIM_CALIBRATION 1

void error_handler(void);

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    printf("IIM test start\n");

    if (!iim_init()) {
        printf("IIM init failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    printf("IIM init OK\n");

#if USE_IIM_CALIBRATION
    printf("Keep board still and flat. Calibrating...\n");

    if (!iim_calibrate()) {
        printf("IIM calibration failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    printf("IIM calibration OK\n");
#else
    printf("Using default calibration values\n");
#endif

    while (true) {
        iim_sample_t s;
        iim_poll_status_t st = iim_poll_sample(&s);

        if (st == IIM_POLL_OK) {
            static uint32_t div = 0;

            if (++div >= 100) {
                div = 0;

                printf(
                    "accel: %.3f %.3f %.3f g | "
                    "gyro: %.3f %.3f %.3f dps | "
                    "raw acc: %d %d %d | "
                    "raw gyro: %d %d %d | "
                    "temp: %.2f %.2f C\n",

                    s.accel_x_g,
                    s.accel_y_g,
                    s.accel_z_g,

                    s.gyro_x_dps,
                    s.gyro_y_dps,
                    s.gyro_z_dps,

                    s.accel_x_raw,
                    s.accel_y_raw,
                    s.accel_z_raw,

                    s.gyro_x_raw,
                    s.gyro_y_raw,
                    s.gyro_z_raw,

                    s.temp1_c,
                    s.temp2_c
                );
            }
        } else if (st == IIM_POLL_ERROR) {
            printf("IIM poll error\n");
        }

        sleep_ms(1);
    }
}

void error_handler(void) {
	gpio_put(BLUE_LED, 0);
	gpio_put(RED_LED, 1);
}
