#ifndef IIM20670_H
#define IIM20670_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

#define IIM_SPI_PORT spi1
#define IIM_SPI_BAUDRATE 50000u

#define IIM_PIN_MISO 8
#define IIM_PIN_CS   9
#define IIM_PIN_SCK  10
#define IIM_PIN_MOSI 11
#define IIM_PIN_RESET -1

#define IIM_SAMPLE_PERIOD_US 1000u

#define IIM_DEFAULT_ACCEL_LSB_PER_G 4700.0f // empirical calibration at lab
#define IIM_DEFAULT_GYRO_LSB_PER_DPS 50.0f
#define IIM_DEFAULT_GYRO_X_BIAS 500.0f
#define IIM_DEFAULT_GYRO_Y_BIAS 900.0f
#define IIM_DEFAULT_GYRO_Z_BIAS 60.0f

typedef enum {
    IIM_POLL_OK = 0,
    IIM_POLL_NO_DATA,
    IIM_POLL_ERROR
} iim_poll_status_t;

typedef struct {
    uint64_t timestamp_us;

    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;

    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;

    int16_t temp1_raw;
    int16_t temp2_raw;

    float accel_x_g;
    float accel_y_g;
    float accel_z_g;

    float gyro_x_dps;
    float gyro_y_dps;
    float gyro_z_dps;

    float temp1_c;
    float temp2_c;

    uint32_t error_count;
} iim_sample_t;

bool iim_init(void);
bool iim_calibrate(void);
iim_poll_status_t iim_poll_sample(iim_sample_t *sample);

#endif
