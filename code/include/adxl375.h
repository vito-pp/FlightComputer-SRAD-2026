#ifndef ADXL375_H
#define ADXL375_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

/**
 * @brief ADXL375 hardware and conversion settings.
 */
#define ADXL375_I2C        i2c1
#define ADXL375_SDA_GPIO    2
#define ADXL375_SCL_GPIO    3
#define ADXL375_I2C_BAUD   400000u // 400kHz

/**
 * @brief ADXL375 I2C address.
 *
 * Use 0x53 when ALT ADDRESS is low, or change this to 0x1D when ALT ADDRESS
 * is tied high.
 */
#define ADXL375_ADDR 0x53

/**
 * @brief ADXL375 output sample rate in Hz.
 *
 * Supported values here are 100, 200, 400, and 800 Hz. Higher rates increase
 * bus traffic because every ready sample is read over I2C.
 */
#define ADXL375_SAMPLE_RATE_HZ 100u

#if ADXL375_SAMPLE_RATE_HZ == 100u
#define ADXL375_BW_RATE_REG 0x0A
#elif ADXL375_SAMPLE_RATE_HZ == 200u
#define ADXL375_BW_RATE_REG 0x0B
#elif ADXL375_SAMPLE_RATE_HZ == 400u
#define ADXL375_BW_RATE_REG 0x0C
#elif ADXL375_SAMPLE_RATE_HZ == 800u
#define ADXL375_BW_RATE_REG 0x0D
#else
#error "Unsupported ADXL375_SAMPLE_RATE_HZ. Use 100, 200, 400, or 800."
#endif

/**
 * @brief Conversion factor from raw LSB to g.
 *
 * The nominal value is close to 20.5 mg/LSB, but this project keeps the
 * empirical divisor here so it can be adjusted after bench calibration.
 */
#define ADXL375_LSB_PER_G 20.5f

/**
 * @brief Number of raw samples averaged by adxl375_calibrate().
 */
#define ADXL375_CAL_SAMPLES 100u

/**
 * @brief Delay between ADXL375 calibration samples.
 */
#define ADXL375_CAL_SAMPLE_DELAY_MS (1000u / ADXL375_SAMPLE_RATE_HZ)

/**
 * @brief Per-transfer I2C timeout, cause I2C writing is blocking on the pico-sdk.
 *
 * Keep this short and sweet. A write should take more than a couple tens of us.
 * */
#define ADXL375_I2C_TIMEOUT_US 1000u

/**
 * @brief Single ADXL375 acceleration sample.
 */
typedef struct {
	float x_g;     /**< X-axis acceleration in g */
	float y_g;     /**< Y-axis acceleration in g */
	float z_g;     /**< Z-axis acceleration in g */
} adxl375_sample_t;

/**
 * @brief Result codes for polling reads.
 */
typedef enum {
	ADXL375_POLL_NO_DATA = 0, /**< No new sample available */
	ADXL375_POLL_OK,          /**< New sample successfully read */
	ADXL375_POLL_ERROR        /**< I2C or device error */
} adxl375_poll_result_t;

/**
 * @brief Initialize the ADXL375 accelerometer using the header macros.
 *
 * Configures I2C, verifies the device ID, sets the sample rate, and enables
 * measurement mode. This driver owns one ADXL375 instance.
 *
 * @return true if initialization succeeded.
 * @return false if initialization failed.
 */
bool adxl375_init(void);

/**
 * @brief Calibrate ADXL375 board-frame acceleration offsets at startup.
 *
 * This averages raw XYZ samples with the same board-axis mapping used by
 * adxl375_poll_sample(). X and Y are zeroed from the still reading, while Z
 * uses the measured +Z/-Z midpoint and span so both signs report near +/-1 g.
 *
 * @return true if calibration succeeded.
 * @return false if a sensor read failed or the measured vector was invalid.
 */
bool adxl375_calibrate(void);

/**
 * @brief Poll the ADXL375 for a new acceleration sample.
 *
 * This checks the DATA_READY bit and returns immediately if no new sample is
 * available. The output g values use the calibrated board-frame correction.
 *
 * @param sample Pointer to output sample structure.
 *
 * @return ADXL375_POLL_OK if a new sample was read.
 * @return ADXL375_POLL_NO_DATA if no new sample is available.
 * @return ADXL375_POLL_ERROR on communication or device failure.
 *
 * NOTE: x and y axis are changed to adapt it to this projects board. See the
 * adxl375.c, line 219
 */
adxl375_poll_result_t adxl375_poll_sample(adxl375_sample_t *sample);

#endif
