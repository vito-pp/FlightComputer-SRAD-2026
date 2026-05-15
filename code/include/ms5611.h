#ifndef MS5611_H
#define MS5611_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

typedef enum {
    MS5611_OK = 0,
    MS5611_ERR_I2C = -1,
    MS5611_ERR_PROM_CRC = -2,
    MS5611_ERR_BAD_PARAM = -3,
    MS5611_ERR_NOT_READY = -4
} ms5611_status_t;

typedef enum {
    MS5611_OSR_256  = 0,
    MS5611_OSR_512  = 1,
    MS5611_OSR_1024 = 2,
    MS5611_OSR_2048 = 3,
    MS5611_OSR_4096 = 4
} ms5611_osr_t;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;

    uint16_t prom[8];

    uint32_t d1_raw;
    uint32_t d2_raw;

    int32_t temperature_centi_c;   // °C * 100
    int32_t pressure_centi_mbar;   // mbar * 100

    ms5611_osr_t osr;

    bool sample_ready;

    uint8_t state;
    absolute_time_t conversion_deadline;
} ms5611_t;

/**
 * @brief Initialize MS5611 on a given I2C bus.
 *
 * Reads PROM coefficients and checks CRC.
 *
 * @param dev Driver instance.
 * @param i2c I2C instance, e.g. i2c1.
 * @param addr I2C address, usually 0x76 or 0x77.
 * @param osr Oversampling ratio.
 */
ms5611_status_t ms5611_init(ms5611_t *dev, i2c_inst_t *i2c, uint8_t addr, ms5611_osr_t osr);

/**
 * @brief Non-blocking polling state machine.
 *
 * Call this frequently from the main loop. It alternates D1 pressure and D2
 * temperature conversions, computes compensated pressure/temperature, and marks
 * a new sample ready.
 */
ms5611_status_t ms5611_poll(ms5611_t *dev);

/**
 * @brief Returns true when a new compensated sample is available.
 */
bool ms5611_sample_ready(const ms5611_t *dev);

/**
 * @brief Clear sample-ready flag after consuming the sample.
 */
void ms5611_clear_sample_ready(ms5611_t *dev);

/**
 * @brief Get compensated pressure in mbar.
 */
float ms5611_get_pressure_mbar(const ms5611_t *dev);

/**
 * @brief Get compensated temperature in Celsius.
 */
float ms5611_get_temperature_c(const ms5611_t *dev);

/**
 * @brief Get altitude in meters using the barometric formula.
 *
 * @param sea_level_mbar Reference pressure. Use 1013.25 for standard atmosphere,
 * or a local QNH value for better absolute altitude.
 */
float ms5611_get_altitude_m(const ms5611_t *dev, float sea_level_mbar);

#endif
