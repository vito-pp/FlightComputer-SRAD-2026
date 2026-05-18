#ifndef MS5611_H
#define MS5611_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

/**
 * @brief MS5611 hardware settings.
 */
#define MS5611_I2C      i2c1
#define MS5611_SDA_PIN  2
#define MS5611_SCL_PIN  3
#define MS5611_I2C_BAUD 400000u

/**
 * @brief Per-transfer I2C timeout.
 *
 * Keep this short so a bad I2C bus does not freeze the flight loop.
 */
#define MS5611_I2C_TIMEOUT_US 1000u

/**
 * @brief MS5611 I2C address.
 */
#define MS5611_ADDR 0x76

/**
 * @brief MS5611 oversampling ratio.
 *
 * Higher OSR improves pressure resolution but increases conversion time. The
 * polling API stays non-blocking; higher OSR values simply require more calls
 * returning MS5611_POLL_NO_DATA before a full pressure/temperature sample is
 * ready.
 */
typedef enum {
    MS5611_OSR_256  = 0,
    MS5611_OSR_512  = 1,
    MS5611_OSR_1024 = 2,
    MS5611_OSR_2048 = 3,
    MS5611_OSR_4096 = 4
} ms5611_osr_t;

/**
 * @brief Oversampling ratio used by ms5611_init().
 *
 * The MS5611 has no fixed sample-rate setting in this driver. The effective
 * rate is set by OSR conversion time and how often ms5611_poll_sample() is
 * called.
 */
#define MS5611_OSR MS5611_OSR_4096

/**
 * @brief Number of pressure samples averaged during ms5611_calibrate().
 *
 * More samples make the launch-site pressure reference less noisy, but keep
 * the program inside calibration longer.
 */
#define MS5611_CAL_SAMPLES 200u

/**
 * @brief Result codes returned by ms5611_poll_sample().
 */
typedef enum {
    MS5611_POLL_NO_DATA = 0, /**< Conversion is still in progress or no full sample is ready. */
    MS5611_POLL_OK,          /**< A compensated pressure/temperature/altitude sample was read. */
    MS5611_POLL_ERROR        /**< Invalid parameter, I2C failure, or invalid driver state. */
} ms5611_poll_result_t;

/**
 * @brief Compensated MS5611 barometer sample.
 *
 * pressure_mbar and temperature_c are compensated using the PROM coefficients
 * read during ms5611_init(). altitude_m is relative to the pressure baseline
 * captured by ms5611_calibrate(); before calibration it is reported as 0.
 */
typedef struct {
    float pressure_mbar; /**< Compensated pressure in mbar. */
    float temperature_c; /**< Compensated temperature in degrees Celsius. */
    float altitude_m;    /**< Altitude in meters relative to the calibration baseline. */
} ms5611_sample_t;

/**
 * @brief Initialize the MS5611 driver using the header macros.
 *
 * Configures the driver's I2C bus/pins, resets the sensor, reads the PROM
 * coefficients, and validates the PROM CRC. This driver owns a single MS5611
 * instance internally, matching the simple init/calibrate/poll_sample pattern
 * used by the other sensors.
 *
 * @return true if the sensor initialized and PROM CRC validation passed.
 * @return false on invalid configuration, I2C failure, or PROM CRC failure.
 */
bool ms5611_init(void);

/**
 * @brief Capture the launch-site pressure baseline for relative altitude.
 *
 * Averages a fixed number of pressure samples and stores that average as the
 * 0 m altitude reference. This intentionally forces altitude_m to 0 m at
 * startup/reference time. Pressure changes after that are reported relative to
 * this baseline. In normal flight use, call this at the pad.
 *
 * @return true if the pressure baseline was captured successfully.
 * @return false if polling the sensor failed during calibration.
 */
bool ms5611_calibrate(void);

/**
 * @brief Poll the MS5611 conversion state machine for a new sample.
 *
 * This function is non-blocking at the application level. Call it frequently
 * from the main loop; it starts pressure/temperature conversions, waits for
 * their deadlines across later calls, computes compensation, and writes one
 * complete sample when ready.
 *
 * IMPORTANT: pressure has a little drift (~microbars per second) but
 * altitude is very suceptible to this change, so it has a larger drift (meters per 
 * minute). TODO: add filtering to overcome the drift
 *
 * @param sample Pointer to the output sample structure.
 *
 * @return MS5611_POLL_OK when sample contains new data.
 * @return MS5611_POLL_NO_DATA when conversions are still in progress.
 * @return MS5611_POLL_ERROR on NULL sample, I2C failure, or invalid state.
 */
ms5611_poll_result_t ms5611_poll_sample(ms5611_sample_t *sample);

#endif
