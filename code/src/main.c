#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "drvs/iim20670.h"
#include "drvs/buzzer.h"
#include "drvs/ms5611.h"
#include "drvs/adxl375.h"
#include "drvs/adc_bat_lvl.h"
#include "frame_ring_buffer.h"
#include "drvs/sd_logger.h"
#include "drvs/xbee.h"
#include "app_crc.h"
// #include "drvs/max_m10s.h" // it doesnt work :(

#define EVER (;;) // forever ever, baby...

// GPIOs
#define BLUE_LED 26
#define RED_LED 27

// Masks for freshness bitfield
#define FRAME_IMU_FRESH   (1u << 0)
#define FRAME_ADXL_FRESH  (1u << 1)
#define FRAME_BARO_FRESH  (1u << 2)
#define FRAME_GNSS_FRESH  (1u << 3)

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

typedef struct {
	bool fresh;
	uint16_t sample_mv;
} latest_battery_t;

// typedef struct {
// 	bool fresh;
// 	gnss_sample_t sample;
// } latest_gnss_t;

// frame ring buffer for all readings and frame counter
static frame_ring_t frame_rb = {0};
static uint32_t frame_number = 0;

int main(void)
{
	buzz_init();
	// buzz_alarm(true);
	gpio_init(BLUE_LED);
	gpio_set_dir(BLUE_LED, GPIO_OUT);
	gpio_put(BLUE_LED, 1);

	adc_bat_lvl_init();

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

	// if (!max_m10s_init()) {
	// 	printf("GNSS init failed\n");
	// 	error_handler();
	// }
	// printf("GNSS init OK\n");

	latest_imu_t latest_imu = {0};
	latest_adxl_t latest_adxl = {0};
	latest_baro_t latest_baro = {0};
	latest_battery_t latest_battery = {0};
	// latest_gnss_t latest_gnss = {0};

	uint64_t next_frame_us = time_us_64();

	multicore_launch_core1(core1_entry);

	// sleep_ms(1000);
	buzz_alarm(false);
	gpio_put(BLUE_LED, 0);

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

		uint16_t bat_tmp;
		if (adc_bat_lvl_poll(&bat_tmp) == BAT_LVL_POLL_OK) {
			latest_battery.sample_mv = bat_tmp;
			latest_battery.fresh = true;
		}

		// gnss_sample_t gnss_tmp;
		// if (max_m10s_poll_sample(&gnss_tmp) == MAX_M10S_POLL_OK) {
		// 	latest_gnss.sample = gnss_tmp;
		// 	latest_gnss.fresh = true;
		// }

		// if time elapsed, create and push the frame
		if ((int64_t)(now - next_frame_us) >= 0) {
			next_frame_us += FRAME_PERIOD_US;

			sensor_frame_t frame = {0};

			frame.sync_word = 0xA55A;
			frame.frame_number = frame_number++;
			frame.timestamp_us = now;

			frame.imu = latest_imu.sample;

			frame.adxl = latest_adxl.sample;

			frame.baro = latest_baro.sample;

			// frame.gnss = latest_gnss.sample;
			frame.battery_mv = latest_battery.sample_mv;

			// /* Dummy GNSS values */
			// frame.gnss.iTOW_ms = 0;
			// frame.gnss.lon_deg_e7 = -583815923;  // -58.3815923 deg
			// frame.gnss.lat_deg_e7 = -346037220;  // -34.6037220 deg
			// frame.gnss.height_mm = 25000;        // 25 m
			// frame.gnss.hMSL_mm = 25000;
			// frame.gnss.velN_mm_s = 0;
			// frame.gnss.velE_mm_s = 0;
			// frame.gnss.velD_mm_s = 0;
			// frame.gnss.gSpeed_mm_s = 0;
			// frame.gnss.hAcc_mm = 999999;
			// frame.gnss.vAcc_mm = 999999;
			// frame.gnss.sAcc_mm_s = 999999;
			// frame.gnss.fixType = 0;              // no fix
			// frame.gnss.numSV = 0;
			// frame.gnss.flags = 0;

			if (latest_imu.fresh) {
				frame.freshness |= FRAME_IMU_FRESH;
			}

			if (latest_adxl.fresh) {
				frame.freshness |= FRAME_ADXL_FRESH;
			}

			if (latest_baro.fresh) {
				frame.freshness |= FRAME_BARO_FRESH;
			}

			// if (latest_gnss.fresh) {
			// 	frame.freshness |= FRAME_GNSS_FRESH;
			// }

			latest_imu.fresh = false; 
			latest_adxl.fresh = false;
			latest_baro.fresh = false;
			latest_battery.fresh = false;
			// latest_gnss.fresh = false;

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
			frame.crc16 = crc16_ccitt_false(&frame);

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
	for EVER {
		sleep_ms(1000);
		printf("Error loop");
	}
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
