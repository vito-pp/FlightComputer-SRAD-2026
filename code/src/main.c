#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "iim20670.h"
#include "buzzer.h"
#include "ms5611.h"
#include "adxl375.h"
#include "frame_ring_buffer.h"
#include "sd_logger.h"
#include "xbee.h"

#define EVER (;;) // forever ever, baby...

// GPIOs
#define BLUE_LED 26
#define RED_LED 27

// Masks for freshness bitfield
#define FRAME_IMU_FRESH   (1u << 0)
#define FRAME_ADXL_FRESH  (1u << 1)
#define FRAME_BARO_FRESH  (1u << 2)

#define FRAME_PERIOD_US 500000 // 2 Hz

void core1_entry(void);
void error_handler(void);
void serial_debug_print(sensor_frame_t *frame);
void log_frame_to_sd(sensor_frame_t *frame);

typedef struct {
	bool fresh;
	iim_sample_t sample;
} latest_imu_t;

typedef struct {
	bool fresh;
	adxl375_sample_t sample;
} latest_adxl_t;

typedef struct {
	bool fresh;
	ms5611_sample_t sample;
} latest_baro_t;

// frame ring buffer for all readings
static frame_ring_t frame_rb = {0};

int main(void)
{
	stdio_init_all();
	sleep_ms(2000);

	if (!sd_logger_init()) {
		printf("SD logger init failed\n");
		error_handler();
	}
	printf("SD logger init OK\n");

	xbee_init();
	printf("XBee init OK\n");

	if (!(ms5611_init() && iim_init() && adxl375_init())) {
		printf("Sensors init failed\n");
		error_handler();
	}
	printf("Sensors init OK\n");

	printf("Calibrating barometer, IMU, and ADXL...\n");
	if (!(ms5611_calibrate() && iim_calibrate() && adxl375_calibrate())) {
		printf("Calibration failed\n");
		error_handler();
	}
	printf("Calibration done\n");

	latest_imu_t latest_imu = {0};
	latest_adxl_t latest_adxl = {0};
	latest_baro_t latest_baro = {0};

	uint64_t next_frame_us = time_us_64();

	multicore_launch_core1(core1_entry);

	for EVER {
		uint64_t now = time_us_64();

		iim_sample_t imu_tmp;
		if (iim_poll_sample(&imu_tmp) == IIM_POLL_OK) {
			latest_imu.sample = imu_tmp;
			latest_imu.fresh = true;
		}

		adxl375_sample_t adxl_tmp;
		if (adxl375_poll_sample(&adxl_tmp) == ADXL375_POLL_OK) {
			latest_adxl.sample = adxl_tmp;
			latest_adxl.fresh = true;
		}

		ms5611_sample_t baro_tmp;
		if (ms5611_poll_sample(&baro_tmp) == MS5611_POLL_OK) {
			latest_baro.sample = baro_tmp;
			latest_baro.fresh = true;
		}

		// if time elapsed, create and push the frame
		if ((int64_t)(now - next_frame_us) >= 0) {
			next_frame_us += FRAME_PERIOD_US;

			sensor_frame_t frame = {0};
			frame.timestamp_us = now;

			frame.imu = latest_imu.sample;

			frame.adxl = latest_adxl.sample;

			frame.baro = latest_baro.sample;

			if (latest_imu.fresh) {
				frame.freshness |= FRAME_IMU_FRESH;
			}

			if (latest_adxl.fresh) {
				frame.freshness |= FRAME_ADXL_FRESH;
			}

			if (latest_baro.fresh) {
				frame.freshness |= FRAME_BARO_FRESH;
			}

			latest_imu.fresh = false; latest_adxl.fresh = false;
			latest_baro.fresh = false;

			frame_ring_push(&frame_rb, &frame);
		}

		tight_loop_contents();
	}
}

void core1_entry(void) 
{
	sensor_frame_t frame;

	for EVER {
		if (frame_ring_pop(&frame_rb, &frame)) {
			serial_debug_print(&frame);
			log_frame_to_sd(&frame);
			xbee_transmit(&frame, sizeof(frame));	
		}
		else {
			tight_loop_contents();
		}
	}
}

void error_handler(void) 
{
	// gpio_put(BLUE_LED, 0);
	// gpio_put(RED_LED, 1);
	for EVER { sleep_ms(1000); }
}

void serial_debug_print(sensor_frame_t *frame) 
{
	printf(
		"\r[%llu us] "
		"IMU%s | "
		"gx: %.2f gy: %.2f gz: %.2f dps | "
		"ax: %.2f ay: %.2f az: %.2f g || "
		"ADXL%s | "
		"ax: %.2f ay: %.2f az: %.2f g || "
		"BARO%s | "
		"P: %.2f mbar T: %.2f C Alt: %.2f m     ",

		(unsigned long long)frame->timestamp_us,

		(frame->freshness & FRAME_IMU_FRESH) ? "*" : "",

		frame->imu.gyro_x_dps,
		frame->imu.gyro_y_dps,
		frame->imu.gyro_z_dps,

		frame->imu.accel_x_g,
		frame->imu.accel_y_g,
		frame->imu.accel_z_g,

		(frame->freshness & FRAME_ADXL_FRESH) ? "*" : "",

		frame->adxl.x_g,
		frame->adxl.y_g,
		frame->adxl.z_g,

		(frame->freshness & FRAME_BARO_FRESH) ? "*" : "",

		frame->baro.pressure_mbar,
		frame->baro.temperature_c,
		frame->baro.altitude_m
	);

	fflush(stdout);
}

void log_frame_to_sd(sensor_frame_t *frame)
{
	if (!sd_logger_append_frame_csv(frame)) {
		printf("\nSD frame log failed\n");
		fflush(stdout);
	}
}
