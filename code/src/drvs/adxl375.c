#include "drvs/adxl375.h"

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include <math.h>

#define REG_DEVID          0x00
#define REG_BW_RATE        0x2C
#define REG_POWER_CTL      0x2D
#define REG_INT_ENABLE     0x2E
#define REG_INT_SOURCE     0x30
#define REG_DATA_FORMAT    0x31
#define REG_DATAX0         0x32

#define ADXL375_DEVID      0xE5

#define INT_DATA_READY     0x80

static float g_accel_lsb_per_g = ADXL375_LSB_PER_G;

static bool write_reg(uint8_t reg, uint8_t value) {
	uint8_t buf[2] = { reg, value };

	int ret = i2c_write_timeout_us(
		ADXL375_I2C,
		ADXL375_ADDR,
		buf,
		2,
		false,
		ADXL375_I2C_TIMEOUT_US
	);

	return ret == 2;
}

static bool read_reg(uint8_t reg, uint8_t *value) {
	int ret = i2c_write_timeout_us(
		ADXL375_I2C,
		ADXL375_ADDR,
		&reg,
		1,
		true,
		ADXL375_I2C_TIMEOUT_US
	);

	if (ret != 1) {
		return false;
	}

	ret = i2c_read_timeout_us(
		ADXL375_I2C,
		ADXL375_ADDR,
		value,
		1,
		false,
		ADXL375_I2C_TIMEOUT_US
	);

	return ret == 1;
}

static bool read_regs(uint8_t reg, uint8_t *buf, size_t len) {
	int ret = i2c_write_timeout_us(
		ADXL375_I2C,
		ADXL375_ADDR,
		&reg,
		1,
		true,
		ADXL375_I2C_TIMEOUT_US
	);

	if (ret != 1) {
		return false;
	}

	ret = i2c_read_timeout_us(
		ADXL375_I2C,
		ADXL375_ADDR,
		buf,
		len,
		false,
		ADXL375_I2C_TIMEOUT_US
	);

	return ret == (int)len;
}

static bool read_raw_axes(int16_t *x, int16_t *y, int16_t *z) {
	uint8_t data[6];

	if (!read_regs(REG_DATAX0, data, sizeof(data))) {
		return false;
	}

	*x = (int16_t)((uint16_t)data[1] << 8 | data[0]);
	*y = (int16_t)((uint16_t)data[3] << 8 | data[2]);
	*z = (int16_t)((uint16_t)data[5] << 8 | data[4]);

	return true;
}

bool adxl375_init(void) {
	i2c_init(ADXL375_I2C, ADXL375_I2C_BAUD);

	gpio_set_function(ADXL375_SDA_GPIO, GPIO_FUNC_I2C);
	gpio_set_function(ADXL375_SCL_GPIO, GPIO_FUNC_I2C);

	gpio_pull_up(ADXL375_SDA_GPIO);
	gpio_pull_up(ADXL375_SCL_GPIO);

	uint8_t devid = 0;

	if (!read_reg(REG_DEVID, &devid)) {
		return false;
	}

	if (devid != ADXL375_DEVID) {
		return false;
	}

	// Configure while in standby.
	if (!write_reg(REG_POWER_CTL, 0x00)) {
		return false;
	}

	if (!write_reg(REG_BW_RATE, ADXL375_BW_RATE_REG)) {
		return false;
	}

	// Default data format, no self-test.
	if (!write_reg(REG_DATA_FORMAT, 0x00)) {
		return false;
	}

	// Enable DATA_READY internally.
	// Even without physical INT pins, INT_SOURCE can still be polled.
	if (!write_reg(REG_INT_ENABLE, INT_DATA_READY)) {
		return false;
	}

	// Enter measurement mode: MEASURE bit = D3.
	if (!write_reg(REG_POWER_CTL, 0x08)) {
		return false;
	}

	sleep_ms(10); // because of the wake-up time of the adxl

	return true;
}

bool adxl375_calibrate(void) {
	int64_t x_sum = 0;
	int64_t y_sum = 0;
	int64_t z_sum = 0;

	for (uint16_t i = 0; i < ADXL375_CAL_SAMPLES; i++) {
		int16_t x;
		int16_t y;
		int16_t z;

		if (!read_raw_axes(&x, &y, &z)) {
			return false;
		}

		// These are the same board-axis corrections used for normal samples.
		x_sum += y;
		y_sum += -x;
		z_sum += z;

		sleep_ms(ADXL375_CAL_SAMPLE_DELAY_MS);
	}

	float x_avg = (float)x_sum / ADXL375_CAL_SAMPLES;
	float y_avg = (float)y_sum / ADXL375_CAL_SAMPLES;
	float z_avg = (float)z_sum / ADXL375_CAL_SAMPLES;

	g_accel_lsb_per_g = sqrtf(
		x_avg * x_avg +
		y_avg * y_avg +
		z_avg * z_avg
	);

	return true;
}

adxl375_poll_result_t adxl375_poll_sample(adxl375_sample_t *sample) {
	if (sample == NULL) {
		return ADXL375_POLL_ERROR;
	}

	uint8_t int_source = 0;

	if (!read_reg(REG_INT_SOURCE, &int_source)) {
		return ADXL375_POLL_ERROR;
	}

	if ((int_source & INT_DATA_READY) == 0) {
		return ADXL375_POLL_NO_DATA;
	}

	int16_t x;
	int16_t y;
	int16_t z;

	if (!read_raw_axes(&x, &y, &z)) return ADXL375_POLL_ERROR;

	// this are ad hoc corrections to make the adxl axis match the iim's
	sample->x_g = (float)y / g_accel_lsb_per_g;
	sample->y_g = (float)-x / g_accel_lsb_per_g;
	sample->z_g = (float)z / g_accel_lsb_per_g;

	return ADXL375_POLL_OK;
}
