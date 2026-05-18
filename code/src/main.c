#include <stdio.h>
#include "pico/stdlib.h"
#include "iim20670.h"
#include "buzzer.h"
#include "ms5611.h"
#include "adxl375.h"

#define EVER (;;) // forever, ever, baby...

// GPIOs
#define BLUE_LED 26
#define RED_LED 27

#define USE_IIM_CALIBRATION 1

void error_handler(void);

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    if (!(ms5611_init() && iim_init() && adxl375_init())) {
        printf("Sensors init failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }

    printf("Sensors init OK\n");

    printf("Calibrating barometer and IMU...\n");
    if (!(ms5611_calibrate() && iim_calibrate())) {
        printf("Calibration failed\n");
        while (true) {
            sleep_ms(1000);
        }
    }
    printf("Calibration done\n");

    for EVER {
        ms5611_sample_t baro;
        ms5611_poll_result_t poll_baro = ms5611_poll_sample(&baro);

	adxl375_sample_t accelero;
	adxl375_poll_result_t poll_accelero = adxl375_poll_sample(&accelero);

	iim_sample_t imu;
	iim_poll_status_t poll_imu = iim_poll_sample(&imu);

        if (poll_baro == MS5611_POLL_OK && poll_accelero == ADXL375_POLL_OK
			&& poll_imu == IIM_POLL_OK) {
			printf(
				"P: %.2f mbar | T: %.2f C | Alt: %.2f m | ax: %.2f g | ay: %.2f g | az: %.2f g | ax: %.2f g | ay: %.2f g | az: %.2f g | gx: %.2f dps | gy: %.2f dps | gz: %.2f dps\n",

				baro.pressure_mbar,
				baro.temperature_c,
				baro.altitude_m,

				accelero.x_g,
				accelero.y_g,
				accelero.z_g,

				imu.accel_x_g,
				imu.accel_y_g,
				imu.accel_z_g,

				imu.gyro_x_dps,
				imu.gyro_y_dps,
				imu.gyro_z_dps

				// imu.temp1_c,
				// imu.temp2_c
			);
        }

        tight_loop_contents();
    }
}

void error_handler(void) {
	gpio_put(BLUE_LED, 0);
	gpio_put(RED_LED, 1);
	while (true) { sleep_ms(1000); }
}
