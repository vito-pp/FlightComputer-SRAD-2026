#include "ms5611.h"

#include <math.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define MS5611_CMD_ADC_READ      0x00
#define MS5611_CMD_RESET         0x1E
#define MS5611_CMD_CONVERT_D1    0x40
#define MS5611_CMD_CONVERT_D2    0x50
#define MS5611_CMD_PROM_READ     0xA0

enum {
    MS5611_STATE_START_D1 = 0,
    MS5611_STATE_WAIT_D1,
    MS5611_STATE_START_D2,
    MS5611_STATE_WAIT_D2
};

static const uint32_t osr_delay_us[] = {
    600,    // OSR 256, datasheet max about 0.60 ms
    1170,   // OSR 512
    2280,   // OSR 1024
    4540,   // OSR 2048
    9040    // OSR 4096
};

static uint8_t osr_to_cmd_bits(ms5611_osr_t osr)
{
    return ((uint8_t)osr) * 2;
}

static ms5611_status_t write_cmd(ms5611_t *dev, uint8_t cmd)
{
    int ret = i2c_write_blocking(dev->i2c, dev->addr, &cmd, 1, false);
    return ret == 1 ? MS5611_OK : MS5611_ERR_I2C;
}

static ms5611_status_t read_prom_word(ms5611_t *dev, uint8_t index, uint16_t *out)
{
    uint8_t cmd = MS5611_CMD_PROM_READ + (index * 2);
    uint8_t buf[2];

    int ret = i2c_write_blocking(dev->i2c, dev->addr, &cmd, 1, true);
    if (ret != 1) {
        return MS5611_ERR_I2C;
    }

    ret = i2c_read_blocking(dev->i2c, dev->addr, buf, 2, false);
    if (ret != 2) {
        return MS5611_ERR_I2C;
    }

    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return MS5611_OK;
}

static ms5611_status_t read_adc(ms5611_t *dev, uint32_t *out)
{
    uint8_t cmd = MS5611_CMD_ADC_READ;
    uint8_t buf[3];

    int ret = i2c_write_blocking(dev->i2c, dev->addr, &cmd, 1, true);
    if (ret != 1) {
        return MS5611_ERR_I2C;
    }

    ret = i2c_read_blocking(dev->i2c, dev->addr, buf, 3, false);
    if (ret != 3) {
        return MS5611_ERR_I2C;
    }

    *out = ((uint32_t)buf[0] << 16) |
           ((uint32_t)buf[1] << 8)  |
           ((uint32_t)buf[2]);

    return MS5611_OK;
}

/*
 * CRC-4 check from MS5611 datasheet/application-note style algorithm.
 * prom[7] low nibble contains the stored CRC.
 */
static uint8_t crc4(uint16_t prom[8])
{
    uint16_t n_rem = 0;
    uint16_t crc_read = prom[7] & 0x000F;

    prom[7] &= 0xFF00;

    for (uint8_t cnt = 0; cnt < 16; cnt++) {
        if (cnt & 1) {
            n_rem ^= prom[cnt >> 1] & 0x00FF;
        } else {
            n_rem ^= prom[cnt >> 1] >> 8;
        }

        for (uint8_t bit = 0; bit < 8; bit++) {
            if (n_rem & 0x8000) {
                n_rem = (n_rem << 1) ^ 0x3000;
            } else {
                n_rem <<= 1;
            }
        }
    }

    prom[7] |= crc_read;
    return (n_rem >> 12) & 0x000F;
}

static void compensate(ms5611_t *dev)
{
    const int64_t C1 = dev->prom[1];
    const int64_t C2 = dev->prom[2];
    const int64_t C3 = dev->prom[3];
    const int64_t C4 = dev->prom[4];
    const int64_t C5 = dev->prom[5];
    const int64_t C6 = dev->prom[6];

    int64_t D1 = dev->d1_raw;
    int64_t D2 = dev->d2_raw;

    int64_t dT = D2 - (C5 << 8);
    int64_t TEMP = 2000 + ((dT * C6) >> 23);

    int64_t OFF  = (C2 << 16) + ((C4 * dT) >> 7);
    int64_t SENS = (C1 << 15) + ((C3 * dT) >> 8);

    /*
     * Second-order temperature compensation.
     * Important below 20°C, especially below -15°C.
     */
    int64_t T2 = 0;
    int64_t OFF2 = 0;
    int64_t SENS2 = 0;

    if (TEMP < 2000) {
        int64_t t = TEMP - 2000;
        T2 = (dT * dT) >> 31;
        OFF2 = (5 * t * t) >> 1;
        SENS2 = (5 * t * t) >> 2;

        if (TEMP < -1500) {
            int64_t t2 = TEMP + 1500;
            OFF2 += 7 * t2 * t2;
            SENS2 += (11 * t2 * t2) >> 1;
        }
    } else {
        int64_t t = TEMP - 2000;
        T2 = 0;
        OFF2 = 0;
        SENS2 = (5 * t * t) >> 3;
    }

    TEMP -= T2;
    OFF -= OFF2;
    SENS -= SENS2;

    int64_t P = (((D1 * SENS) >> 21) - OFF) >> 15;

    dev->temperature_centi_c = (int32_t)TEMP;
    dev->pressure_centi_mbar = (int32_t)P;
}

ms5611_status_t ms5611_init(ms5611_t *dev, i2c_inst_t *i2c, uint8_t addr, ms5611_osr_t osr)
{
    if (!dev || !i2c || osr > MS5611_OSR_4096) {
        return MS5611_ERR_BAD_PARAM;
    }

    memset(dev, 0, sizeof(*dev));

    dev->i2c = i2c;
    dev->addr = addr;
    dev->osr = osr;
    dev->state = MS5611_STATE_START_D1;

    ms5611_status_t st = write_cmd(dev, MS5611_CMD_RESET);
    if (st != MS5611_OK) {
        return st;
    }

    /*
     * Reset reload time is around 2.8 ms according to datasheet.
     * This is inside init only, so blocking here is usually acceptable.
     */
    sleep_ms(3);

    for (uint8_t i = 0; i < 8; i++) {
        st = read_prom_word(dev, i, &dev->prom[i]);
        if (st != MS5611_OK) {
            return st;
        }
    }

    uint16_t prom_copy[8];
    memcpy(prom_copy, dev->prom, sizeof(prom_copy));

    uint8_t crc_calc = crc4(prom_copy);
    uint8_t crc_stored = dev->prom[7] & 0x000F;

    if (crc_calc != crc_stored) {
        return MS5611_ERR_PROM_CRC;
    }

    return MS5611_OK;
}

ms5611_status_t ms5611_poll(ms5611_t *dev)
{
    if (!dev) {
        return MS5611_ERR_BAD_PARAM;
    }

    ms5611_status_t st;

    switch (dev->state) {
    case MS5611_STATE_START_D1: {
        uint8_t cmd = MS5611_CMD_CONVERT_D1 + osr_to_cmd_bits(dev->osr);
        st = write_cmd(dev, cmd);
        if (st != MS5611_OK) {
            return st;
        }

        dev->conversion_deadline = make_timeout_time_us(osr_delay_us[dev->osr]);
        dev->state = MS5611_STATE_WAIT_D1;
        return MS5611_OK;
    }

    case MS5611_STATE_WAIT_D1:
        if (!time_reached(dev->conversion_deadline)) {
            return MS5611_ERR_NOT_READY;
        }

        st = read_adc(dev, &dev->d1_raw);
        if (st != MS5611_OK) {
            return st;
        }

        dev->state = MS5611_STATE_START_D2;
        return MS5611_OK;

    case MS5611_STATE_START_D2: {
        uint8_t cmd = MS5611_CMD_CONVERT_D2 + osr_to_cmd_bits(dev->osr);
        st = write_cmd(dev, cmd);
        if (st != MS5611_OK) {
            return st;
        }

        dev->conversion_deadline = make_timeout_time_us(osr_delay_us[dev->osr]);
        dev->state = MS5611_STATE_WAIT_D2;
        return MS5611_OK;
    }

    case MS5611_STATE_WAIT_D2:
        if (!time_reached(dev->conversion_deadline)) {
            return MS5611_ERR_NOT_READY;
        }

        st = read_adc(dev, &dev->d2_raw);
        if (st != MS5611_OK) {
            return st;
        }

        compensate(dev);
        dev->sample_ready = true;

        dev->state = MS5611_STATE_START_D1;
        return MS5611_OK;

    default:
        dev->state = MS5611_STATE_START_D1;
        return MS5611_ERR_BAD_PARAM;
    }
}

bool ms5611_sample_ready(const ms5611_t *dev)
{
    return dev && dev->sample_ready;
}

void ms5611_clear_sample_ready(ms5611_t *dev)
{
    if (dev) {
        dev->sample_ready = false;
    }
}

float ms5611_get_pressure_mbar(const ms5611_t *dev)
{
    return dev ? dev->pressure_centi_mbar / 100.0f : 0.0f;
}

float ms5611_get_temperature_c(const ms5611_t *dev)
{
    return dev ? dev->temperature_centi_c / 100.0f : 0.0f;
}

float ms5611_get_altitude_m(const ms5611_t *dev, float sea_level_mbar)
{
    if (!dev || sea_level_mbar <= 0.0f) {
        return 0.0f;
    }

    float pressure = ms5611_get_pressure_mbar(dev);

    /*
     * International Standard Atmosphere approximation.
     * altitude = 44330 * (1 - (P/P0)^0.1903)
     */
    return 44330.0f * (1.0f - powf(pressure / sea_level_mbar, 0.19029495f));
}
