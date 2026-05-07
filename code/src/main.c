#include <stdio.h>
#include "pico/stdlib.h"
#include "iim20670.h"

// GPIOs
#define BLUE_LED 26  
#define RED_LED 27 

void error_handler(void);

int main(void)
{
    gpio_init(BLUE_LED);
    gpio_set_dir(BLUE_LED, GPIO_OUT);

    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);

    gpio_put(BLUE_LED, 0);
    gpio_put(RED_LED, 0);

    stdio_init_all(); // USB as stdout
    sleep_ms(2000);

    printf("IIM-20670 test start\n");

    if (!iim_init()) {
        printf("IIM init failed\n");
	error_handler();
    }

    printf("IIM init OK\n");

    while (true) {
        iim_sample_t s;

        iim_poll_status_t st = iim_poll_sample(&s);

        if (st == IIM_POLL_OK) {
            printf(
                "t=%llu ax=%d ay=%d az=%d gx=%d gy=%d gz=%d temp=%.2f\n",
                s.timestamp_us,
                s.accel_x,
                s.accel_y,
                s.accel_z,
                s.gyro_x,
                s.gyro_y,
                s.gyro_z,
                iim_temp_raw_to_c(s.temp1)
            );
        } else if (st == IIM_POLL_ERROR) {
            printf("IIM poll error\n");
        }
    }
}

void error_handler(void) {
	gpio_put(BLUE_LED, 0);
	gpio_put(RED_LED, 1);
}
