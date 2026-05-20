#ifndef IIM20670_H
#define IIM20670_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

/**
 * @brief IIM-20670 SPI hardware settings.
 *
 * Edit these macros when the IMU is moved to another SPI peripheral, pinout,
 * baudrate, or reset wiring. Set IIM_GPIO_RESET to -1 when reset is not wired.
 */
#define IIM_SPI_PORT spi1
#define IIM_SPI_BAUDRATE 1000000u // 1MHz

#define IIM_GPIO_MISO  8
#define IIM_GPIO_CS    9
#define IIM_GPIO_SCK   10
#define IIM_GPIO_MOSI  11
#define IIM_GPIO_RESET -1

/**
 * @brief Requested IMU sample period for iim_poll_sample().
 *
 * The polling function returns IIM_POLL_NO_DATA until this period has elapsed.
 * It does not configure an internal sensor ODR; it paces reads in software.
 */
#define IIM_SAMPLE_PERIOD_US 1000u  // 1kHz

/**
 * @brief Number of raw samples averaged by iim_calibrate().
 */
#define IIM_CAL_SAMPLES 200u

/**
 * @brief Delay between calibration samples.
 */
#define IIM_CAL_SAMPLE_DELAY_MS 5u

/**
 * @brief Default conversion and bias values used before calibration.
 *
 * These are fallback/bench values. iim_calibrate() replaces the accel and gyro
 * biases at startup. For accel, it assumes the board is still and flat, then
 * treats the measured Z axis as +1 g.
 */
#define IIM_DEFAULT_ACCEL_LSB_PER_G 4700.0f
#define IIM_DEFAULT_GYRO_LSB_PER_DPS 50.0f
#define IIM_DEFAULT_GYRO_X_BIAS 500.0f
#define IIM_DEFAULT_GYRO_Y_BIAS 900.0f
#define IIM_DEFAULT_GYRO_Z_BIAS 60.0f

/**
 * @brief Result codes returned by iim_poll_sample().
 */
typedef enum {
    IIM_POLL_OK = 0,    /**< New sample was read. */
    IIM_POLL_NO_DATA,   /**< Software sample period has not elapsed yet. */
    IIM_POLL_ERROR      /**< Invalid parameter or SPI/register read failure. */
} iim_poll_status_t;

/**
 * @brief One converted IIM-20670 IMU sample.
 */
typedef struct {
    uint64_t timestamp_us; /**< time_us_64() timestamp of the read. */

    float accel_x_g;     /**< Calibrated X acceleration in g. */
    float accel_y_g;     /**< Calibrated Y acceleration in g. */
    float accel_z_g;     /**< Calibrated Z acceleration in g. */

    float gyro_x_dps;    /**< Calibrated X angular rate in deg/s. */
    float gyro_y_dps;    /**< Calibrated Y angular rate in deg/s. */
    float gyro_z_dps;    /**< Calibrated Z angular rate in deg/s. */

    float temp1_c;       /**< First temperature estimate in degrees Celsius. */
    float temp2_c;       /**< Second temperature estimate in degrees Celsius. */

    uint32_t error_count; /**< Driver error counter copied into the sample. */
} iim_sample_t;

/**
 * @brief Initialize the IIM-20670 driver using the header macros.
 *
 * Configures SPI, optional reset, and checks the fixed-value register. This
 * driver owns one IIM-20670 instance.
 */
bool iim_init(void);

/**
 * @brief Calibrate IMU biases from a still, flat startup position.
 *
 * This averages IIM_CAL_SAMPLES raw reads. The gyro average becomes the zero
 * rate bias (it assumes the board is still when calibrating). On the other 
 * hand, accel coordinates are just normalized, not bias corrected (because, if
 * so, calibration would be position dependent).
 */
bool iim_calibrate(void);

/**
 * @brief Poll for a new IMU sample.
 *
 * Returns IIM_POLL_NO_DATA until IIM_SAMPLE_PERIOD_US has elapsed, then reads
 * and converts one sample using the current calibration values.
 */
iim_poll_status_t iim_poll_sample(iim_sample_t *sample);

#endif
