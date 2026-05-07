#ifndef IIM20670_H
#define IIM20670_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

#ifndef IIM_SPI_PORT
#define IIM_SPI_PORT spi0
#endif

#ifndef IIM_SPI_BAUDRATE
#define IIM_SPI_BAUDRATE 1000000u
#endif

#ifndef IIM_PIN_MISO
#define IIM_PIN_MISO 16
#endif

#ifndef IIM_PIN_CS
#define IIM_PIN_CS   17
#endif

#ifndef IIM_PIN_SCK
#define IIM_PIN_SCK  18
#endif

#ifndef IIM_PIN_MOSI
#define IIM_PIN_MOSI 19
#endif

#ifndef IIM_PIN_RESET
#define IIM_PIN_RESET -1
#endif

#ifndef IIM_SAMPLE_PERIOD_US
#define IIM_SAMPLE_PERIOD_US 1000u
#endif

typedef enum {
    IIM_POLL_OK = 0,
    IIM_POLL_NO_DATA,
    IIM_POLL_ERROR
} iim_poll_status_t;

typedef struct {
    uint64_t timestamp_us;

    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;

    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;

    int16_t accel_x_lr;
    int16_t accel_y_lr;
    int16_t accel_z_lr;

    int16_t temp1;
    int16_t temp2;

    uint32_t error_count;
} iim_sample_t;

bool iim_init(void);
iim_poll_status_t iim_poll_sample(iim_sample_t *sample);

float iim_accel_raw_to_g(int16_t raw);
float iim_accel_lr_raw_to_g(int16_t raw);
float iim_gyro_raw_to_dps(int16_t raw);
float iim_temp_raw_to_c(int16_t raw);

#endif
