#include "iim20670.h"

#include <stddef.h>
// #include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define IIM_REG_GYRO_X       0x00
#define IIM_REG_GYRO_Y       0x01
#define IIM_REG_GYRO_Z       0x02
#define IIM_REG_TEMP1        0x03
#define IIM_REG_ACCEL_X      0x04
#define IIM_REG_ACCEL_Y      0x05
#define IIM_REG_ACCEL_Z      0x06
#define IIM_REG_TEMP2        0x07
#define IIM_REG_ACCEL_X_LR   0x08
#define IIM_REG_ACCEL_Y_LR   0x09
#define IIM_REG_ACCEL_Z_LR   0x0A
#define IIM_REG_FIXED_VALUE  0x0B
#define IIM_REG_MODE         0x19
#define IIM_REG_BANK_SELECT  0x1F
#define IIM_REG_WHOAMI       0x0E

#define IIM_FIXED_VALUE      0xAA55
#define IIM_WHOAMI_VALUE     0x00F3

#define IIM_STATUS_OK        0x01

typedef struct {
	float gyro_x_bias;
	float gyro_y_bias;
	float gyro_z_bias;

	float accel_lsb_per_g;
	float gyro_lsb_per_dps;
} iim_cal_t;

static iim_cal_t g_cal = {
	.gyro_x_bias = IIM_DEFAULT_GYRO_X_BIAS,
	.gyro_y_bias = IIM_DEFAULT_GYRO_Y_BIAS,
	.gyro_z_bias = IIM_DEFAULT_GYRO_Z_BIAS,

	.accel_lsb_per_g = IIM_DEFAULT_ACCEL_LSB_PER_G,
	.gyro_lsb_per_dps = IIM_DEFAULT_GYRO_LSB_PER_DPS,
};

static uint64_t g_next_sample_us = 0;
static uint32_t g_error_count = 0;

static inline void cs_select(void) {
	asm volatile("nop \n nop \n nop");
	gpio_put(IIM_GPIO_CS, 0);
	asm volatile("nop \n nop \n nop");
}

static inline void cs_deselect(void) {
	asm volatile("nop \n nop \n nop");
	gpio_put(IIM_GPIO_CS, 1);
	asm volatile("nop \n nop \n nop");
}

static uint8_t iim_crc24(uint32_t input_data)
{
	uint8_t crc = 0xFF;

	for (int i = 23; i >= 0; i--) {
		uint8_t in_bit = (input_data >> i) & 1u;
		uint8_t crc7 = (crc >> 7) & 1u;

		uint8_t new_crc = 0;
		new_crc |= ((crc >> 6) & 1u) << 7;
		new_crc |= ((crc >> 5) & 1u) << 6;
		new_crc |= ((crc >> 4) & 1u) << 5;
		new_crc |= (((crc >> 3) & 1u) ^ crc7) << 4;
		new_crc |= (((crc >> 2) & 1u) ^ crc7) << 3;
		new_crc |= (((crc >> 1) & 1u) ^ crc7) << 2;
		new_crc |= ((crc >> 0) & 1u) << 1;
		new_crc |= (in_bit ^ crc7) << 0;

		crc = new_crc;
	}

	return crc ^ 0xFF;
}

static uint32_t iim_make_cmd(bool write, uint8_t addr, uint16_t data)
{
	uint32_t word24 = 0;

	word24 |= ((uint32_t)(write ? 1u : 0u)) << 23;
	word24 |= ((uint32_t)(addr & 0x1Fu)) << 18;
	word24 |= ((uint32_t)data) << 0;

	uint8_t crc = iim_crc24(word24);

	return (word24 << 8) | crc;
}

static bool iim_transfer32(uint32_t tx_word, uint32_t *rx_word)
{
	uint8_t tx[4] = {
		(uint8_t)(tx_word >> 24),
		(uint8_t)(tx_word >> 16),
		(uint8_t)(tx_word >> 8),
		(uint8_t)(tx_word >> 0),
	};

	uint8_t rx[4] = {0};

	cs_select();
	int n = spi_write_read_blocking(IIM_SPI_PORT, tx, rx, 4);
	cs_deselect();

	if (n != 4) {
		g_error_count++;
		return false;
	}

	if (rx_word) {
		*rx_word =
			((uint32_t)rx[0] << 24) |
			((uint32_t)rx[1] << 16) |
			((uint32_t)rx[2] << 8)  |
			((uint32_t)rx[3] << 0);
	}

	return true;
}

static uint8_t iim_response_status(uint32_t rx_word)
{
	return (uint8_t)((rx_word >> 24) & 0x03u);
}

static uint16_t iim_response_data(uint32_t rx_word)
{
	return (uint16_t)((rx_word >> 8) & 0xFFFFu);
}

static bool iim_read_reg(uint8_t addr, uint16_t *value)
{
	uint32_t rx;

	uint32_t request = iim_make_cmd(false, addr, 0x0000);
	uint32_t dummy   = iim_make_cmd(false, IIM_REG_FIXED_VALUE, 0x0000);

	if (!iim_transfer32(request, &rx)) {
		return false;
	}

	if (!iim_transfer32(dummy, &rx)) {
		return false;
	}

	uint8_t st = iim_response_status(rx);

	if (st == 0) {
		g_error_count++;
		return false;
	}

	if (value) {
		*value = iim_response_data(rx);
	}

	return true;
}

static bool iim_write_reg(uint8_t addr, uint16_t value)
{
	uint32_t rx;
	uint32_t cmd = iim_make_cmd(true, addr, value);

	if (!iim_transfer32(cmd, &rx)) {
		return false;
	}

	return true;
}

static bool iim_unlock_bank_select(void)
{
	uint16_t mode;

	if (!iim_read_reg(IIM_REG_MODE, &mode)) return false;
	mode = (mode & ~0x0007u) | 0x0002u;
	if (!iim_write_reg(IIM_REG_MODE, mode)) return false;

	if (!iim_read_reg(IIM_REG_MODE, &mode)) return false;
	mode = (mode & ~0x0007u) | 0x0001u;
	if (!iim_write_reg(IIM_REG_MODE, mode)) return false;

	if (!iim_read_reg(IIM_REG_MODE, &mode)) return false;
	mode = (mode & ~0x0007u) | 0x0004u;
	if (!iim_write_reg(IIM_REG_MODE, mode)) return false;

	return true;
}

static bool iim_select_bank(uint16_t bank)
{
	return iim_write_reg(IIM_REG_BANK_SELECT, bank);
}

static bool iim_read_sample_block(iim_sample_t *s)
{
	if (!s) return false;

	uint16_t v;

	if (!iim_read_reg(IIM_REG_ACCEL_X, &v)) return false;
	int16_t accel_x_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_ACCEL_Y, &v)) return false;
	int16_t accel_y_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_ACCEL_Z, &v)) return false;
	int16_t accel_z_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_GYRO_X, &v)) return false;
	int16_t gyro_x_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_GYRO_Y, &v)) return false;
	int16_t gyro_y_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_GYRO_Z, &v)) return false;
	int16_t gyro_z_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_TEMP1, &v)) return false;
	int16_t temp1_raw = (int16_t)v;

	if (!iim_read_reg(IIM_REG_TEMP2, &v)) return false;
	int16_t temp2_raw = (int16_t)v;

	s->accel_x_g = (float)accel_x_raw / g_cal.accel_lsb_per_g;

	s->accel_y_g = (float)accel_y_raw / g_cal.accel_lsb_per_g;

	s->accel_z_g = (float)accel_z_raw / g_cal.accel_lsb_per_g;

	s->gyro_x_dps = ((float)gyro_x_raw - g_cal.gyro_x_bias) /
		g_cal.gyro_lsb_per_dps;

	s->gyro_y_dps = ((float)gyro_y_raw - g_cal.gyro_y_bias) /
		g_cal.gyro_lsb_per_dps;

	s->gyro_z_dps = ((float)gyro_z_raw - g_cal.gyro_z_bias) /
		g_cal.gyro_lsb_per_dps;

	s->temp1_c = 25.0f + ((float)temp1_raw / 20.0f);
	s->temp2_c = 25.0f + ((float)temp2_raw / 20.0f);

	s->error_count = g_error_count;

	return true;
}

bool iim_init(void)
{
	g_error_count = 0;

	spi_init(IIM_SPI_PORT, IIM_SPI_BAUDRATE);

	gpio_set_function(IIM_GPIO_MISO, GPIO_FUNC_SPI);
	gpio_set_function(IIM_GPIO_SCK,  GPIO_FUNC_SPI);
	gpio_set_function(IIM_GPIO_MOSI, GPIO_FUNC_SPI);

	gpio_init(IIM_GPIO_CS);
	gpio_set_dir(IIM_GPIO_CS, GPIO_OUT);
	gpio_put(IIM_GPIO_CS, 1);

	spi_set_format(
		IIM_SPI_PORT,
		8,
		SPI_CPOL_0,
		SPI_CPHA_0,
		SPI_MSB_FIRST
	);

#if IIM_GPIO_RESET >= 0
	gpio_init(IIM_GPIO_RESET);
	gpio_set_dir(IIM_GPIO_RESET, GPIO_OUT);
	gpio_put(IIM_GPIO_RESET, 0);
	sleep_ms(30);
	gpio_put(IIM_GPIO_RESET, 1);
#endif

	sleep_ms(200);

	uint16_t fixed = 0;

	if (!iim_read_reg(IIM_REG_FIXED_VALUE, &fixed)) {
		// printf("IIM init failed: fixed register read failed\n");
		return false;
	}

	// printf("IIM fixed = 0x%04X\n", fixed);

	if (fixed != IIM_FIXED_VALUE) {
		// printf("IIM init failed: expected 0x%04X, got 0x%04X\n",
		// IIM_FIXED_VALUE,
		// fixed);
		return false;
	}

	// printf("IIM init OK: fixed register matched\n");

	g_next_sample_us = time_us_64();

	return true;
}

bool iim_calibrate(void)
{
	int64_t ax_sum = 0;
	int64_t ay_sum = 0;
	int64_t az_sum = 0;

	int64_t gx_sum = 0;
	int64_t gy_sum = 0;
	int64_t gz_sum = 0;

	for (uint16_t i = 0; i < IIM_CAL_SAMPLES; i++) {
		uint16_t v;

		if (!iim_read_reg(IIM_REG_ACCEL_X, &v)) return false;
		ax_sum += (int16_t)v;

		if (!iim_read_reg(IIM_REG_ACCEL_Y, &v)) return false;
		ay_sum += (int16_t)v;

		if (!iim_read_reg(IIM_REG_ACCEL_Z, &v)) return false;
		az_sum += (int16_t)v;

		if (!iim_read_reg(IIM_REG_GYRO_X, &v)) return false;
		gx_sum += (int16_t)v;

		if (!iim_read_reg(IIM_REG_GYRO_Y, &v)) return false;
		gy_sum += (int16_t)v;

		if (!iim_read_reg(IIM_REG_GYRO_Z, &v)) return false;
		gz_sum += (int16_t)v;

		sleep_ms(IIM_CAL_SAMPLE_DELAY_MS);
	}

	float ax_avg = (float)ax_sum / IIM_CAL_SAMPLES;
	float ay_avg = (float)ay_sum / IIM_CAL_SAMPLES;
	float az_avg = (float)az_sum / IIM_CAL_SAMPLES;

	g_cal.accel_lsb_per_g = sqrtf(
		ax_avg * ax_avg +
		ay_avg * ay_avg +
		az_avg * az_avg
	);

	g_cal.gyro_x_bias = (float)gx_sum / IIM_CAL_SAMPLES;
	g_cal.gyro_y_bias = (float)gy_sum / IIM_CAL_SAMPLES;
	g_cal.gyro_z_bias = (float)gz_sum / IIM_CAL_SAMPLES;

	return true;
}

iim_poll_status_t iim_poll_sample(iim_sample_t *sample)
{
	if (!sample) {
		g_error_count++;
		return IIM_POLL_ERROR;
	}

	uint64_t now = time_us_64();

	if ((int64_t)(now - g_next_sample_us) < 0) {
		return IIM_POLL_NO_DATA;
	}

	g_next_sample_us += IIM_SAMPLE_PERIOD_US;

	sample->timestamp_us = now;

	if (!iim_read_sample_block(sample)) {
		return IIM_POLL_ERROR;
	}

	return IIM_POLL_OK;
}
