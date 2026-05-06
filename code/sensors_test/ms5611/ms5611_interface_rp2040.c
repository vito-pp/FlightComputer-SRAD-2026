/**
 * @file      ms5611_interface_rp2040.c
 * @brief     MS5611 RP2040 Pico SDK platform interface implementation
 * @version   1.0.0
 * @date      2026-04-02
 *
 * @details   Implements the LibDriver MS5611 interface callbacks for the
 *            RP2040 using the Pico SDK hardware I2C driver.
 *
 *            The MS5611 uses a command-based I2C protocol (no traditional
 *            register map).  Each operation works like this:
 *
 *            WRITE (commands like Reset, Convert D1/D2):
 *              START → [addr+W] → [command byte] → STOP
 *
 *            READ (ADC result or PROM):
 *              START → [addr+W] → [command byte] → STOP
 *              START → [addr+R] → [data MSB] → ... → [data LSB] → NACK → STOP
 *
 *            The LibDriver passes 8-bit I2C addresses (0xEE / 0xEC).
 *            The Pico SDK expects 7-bit addresses (0x77 / 0x76).
 *            Conversion: addr_7bit = addr_8bit >> 1.
 *
 * @note      SHARED BUS WARNING: I2C1 is shared between the MS5611 and the
 *            ADXL375 on this PCB.  If you run concurrent access from both
 *            cores or from interrupts, you MUST protect the bus with a mutex.
 *            In a single-threaded polling loop this is not necessary.
 */

#include "ms5611_interface_rp2040.h"
#include "driver_ms5611_interface.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#include <stdio.h>
#include <stdarg.h>

/* =========================================================================
 * Internal state
 * ========================================================================= */

/** Flag to track whether this interface layer initialized the I2C bus itself.
 *  If the bus was already initialized by another driver (e.g. ADXL375), we
 *  should NOT deinitialize it when ms5611_interface_iic_deinit() is called. */
static bool s_bus_owned = false;

/* =========================================================================
 * I2C interface
 * ========================================================================= */

/**
 * @brief  Initialize the I2C bus for MS5611 communication.
 *
 * If the bus is already running (e.g. another sensor initialized it first),
 * this function detects that and skips re-initialization, since both the
 * ADXL375 and the MS5611 share I2C1 on this PCB.
 */
uint8_t ms5611_interface_iic_init(void)
{
    /*
     * Check if the SDA pin is already configured as I2C.  If so, the bus
     * was initialized elsewhere and we should not touch it.
     */
    if (gpio_get_function(MS5611_I2C_SDA_PIN) == GPIO_FUNC_I2C)
    {
        s_bus_owned = false;      /* someone else owns the bus */
        return 0;
    }

    /* Initialize the I2C peripheral */
    i2c_init(MS5611_I2C_INSTANCE, MS5611_I2C_FREQ_HZ);

    /* Configure GPIO pins for I2C function */
    gpio_set_function(MS5611_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(MS5611_I2C_SCL_PIN, GPIO_FUNC_I2C);

    /*
     * Enable internal pull-ups as a safety net.  The PCB has external 10 kΩ
     * pull-ups, but enabling the weak internals (~50 kΩ) in parallel doesn't
     * hurt and protects against a missing external resistor.
     */
    gpio_pull_up(MS5611_I2C_SDA_PIN);
    gpio_pull_up(MS5611_I2C_SCL_PIN);

    s_bus_owned = true;           /* we own the bus — we may deinit later */
    return 0;
}

/**
 * @brief  Deinitialize the I2C bus.
 *
 * Only actually deinitializes if this interface layer was the one that
 * initialized the bus.  On a shared bus this prevents accidentally killing
 * communication for the ADXL375.
 */
uint8_t ms5611_interface_iic_deinit(void)
{
    if (s_bus_owned)
    {
        i2c_deinit(MS5611_I2C_INSTANCE);
        gpio_set_function(MS5611_I2C_SDA_PIN, GPIO_FUNC_NULL);
        gpio_set_function(MS5611_I2C_SCL_PIN, GPIO_FUNC_NULL);
        s_bus_owned = false;
    }

    return 0;
}

/**
 * @brief      Read data from the MS5611 via I2C.
 *
 * Protocol on the wire:
 *   1. Write one byte (the command / register) to the device.
 *   2. Read `len` bytes back.
 *
 * @param[in]  addr  8-bit I2C write address from LibDriver (0xEE or 0xEC).
 * @param[in]  reg   Command byte (e.g. 0x00 = ADC read, 0xA0–0xAE = PROM).
 * @param[out] buf   Destination buffer.
 * @param[in]  len   Number of bytes to read (2 for PROM, 3 for ADC).
 * @return     0 success, 1 failure.
 */
uint8_t ms5611_interface_iic_read(uint8_t addr, uint8_t reg,
                                  uint8_t *buf, uint16_t len)
{
    int ret;
    uint8_t addr_7bit = addr >> 1;    /* LibDriver 8-bit → Pico SDK 7-bit */

    /*
     * Step 1: send the command byte.
     * nostop = true  →  hold the bus (repeated start) so we can read next.
     */
    ret = i2c_write_timeout_us(MS5611_I2C_INSTANCE,
                               addr_7bit,
                               &reg, 1,
                               true,                   /* nostop = true */
                               MS5611_I2C_TIMEOUT_US);
    if (ret < 0)
    {
        return 1;    /* write phase failed (NACK / timeout) */
    }

    /*
     * Step 2: clock in the response bytes.
     * nostop = false  →  release the bus with a STOP after reading.
     */
    ret = i2c_read_timeout_us(MS5611_I2C_INSTANCE,
                              addr_7bit,
                              buf, len,
                              false,                   /* nostop = false */
                              MS5611_I2C_TIMEOUT_US);
    if (ret < 0)
    {
        return 1;    /* read phase failed */
    }

    return 0;
}

/**
 * @brief     Write a command (and optional data) to the MS5611 via I2C.
 *
 * Protocol on the wire:
 *   START → [addr+W] → [reg] → [buf[0]] → ... → [buf[len-1]] → STOP
 *
 * For commands like Reset (0x1E) or Convert (0x4x / 0x5x), buf is NULL
 * and len is 0 — only the command byte is sent.
 *
 * @param[in] addr  8-bit I2C write address from LibDriver.
 * @param[in] reg   Command byte.
 * @param[in] buf   Extra data bytes (NULL for command-only writes).
 * @param[in] len   Number of extra data bytes (0 for command-only).
 * @return    0 success, 1 failure.
 */
uint8_t ms5611_interface_iic_write(uint8_t addr, uint8_t reg,
                                   uint8_t *buf, uint16_t len)
{
    int ret;
    uint8_t addr_7bit = addr >> 1;

    if (len == 0)
    {
        /* Command-only write — single byte */
        ret = i2c_write_timeout_us(MS5611_I2C_INSTANCE,
                                   addr_7bit,
                                   &reg, 1,
                                   false,              /* nostop = false */
                                   MS5611_I2C_TIMEOUT_US);
    }
    else
    {
        /*
         * Command + data.  Build a small buffer on the stack so we can
         * send everything in one I2C transaction (one START, one STOP).
         * MS5611 never needs more than a handful of bytes here.
         */
        uint8_t tmp[8];
        uint16_t total = 1 + len;

        if (total > sizeof(tmp))
        {
            return 1;    /* sanity check — should never happen with MS5611 */
        }

        tmp[0] = reg;
        for (uint16_t i = 0; i < len; i++)
        {
            tmp[1 + i] = buf[i];
        }

        ret = i2c_write_timeout_us(MS5611_I2C_INSTANCE,
                                   addr_7bit,
                                   tmp, total,
                                   false,
                                   MS5611_I2C_TIMEOUT_US);
    }

    if (ret < 0)
    {
        return 1;    /* I2C error */
    }

    return 0;
}

/* =========================================================================
 * SPI stubs — not used, MS5611 is on I2C on this PCB
 * ========================================================================= */

uint8_t ms5611_interface_spi_init(void)
{
    return 0;
}

uint8_t ms5611_interface_spi_deinit(void)
{
    return 0;
}

uint8_t ms5611_interface_spi_read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)reg;
    (void)buf;
    (void)len;
    return 0;
}

uint8_t ms5611_interface_spi_write(uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)reg;
    (void)buf;
    (void)len;
    return 0;
}

/* =========================================================================
 * Utility functions
 * ========================================================================= */

/**
 * @brief  Millisecond delay using the Pico SDK.
 */
void ms5611_interface_delay_ms(uint32_t ms)
{
    sleep_ms(ms);
}

/**
 * @brief  Debug print via stdio (USB or UART, depending on your CMake config).
 *
 * Requires `pico_enable_stdio_usb(target 1)` or `pico_enable_stdio_uart(target 1)`
 * in your CMakeLists.txt for output to appear.
 */
void ms5611_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
