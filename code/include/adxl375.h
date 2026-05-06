#ifndef ADXL375_H
#define ADXL375_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Single ADXL375 acceleration sample.
 */
typedef struct {
    int16_t x_raw; /**< Raw X-axis reading */
    int16_t y_raw; /**< Raw Y-axis reading */
    int16_t z_raw; /**< Raw Z-axis reading */

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
 * @brief Initialize the ADXL375 accelerometer.
 *
 * Configures I2C, verifies device ID, and enables measurement mode.
 *
 * @return true if initialization succeeded.
 * @return false if initialization failed.
 */
bool adxl375_init(void);

/**
 * @brief Poll the ADXL375 for a new acceleration sample.
 *
 * Non-blocking at the application level:
 * - returns immediately if no new data is available
 * - reads sensor registers only when data is ready
 *
 * @param sample Pointer to output sample structure.
 *
 * @return ADXL375_POLL_OK if a new sample was read.
 * @return ADXL375_POLL_NO_DATA if no new sample is available.
 * @return ADXL375_POLL_ERROR on communication or device failure.
 */
adxl375_poll_result_t adxl375_poll_sample(adxl375_sample_t *sample);

#endif
