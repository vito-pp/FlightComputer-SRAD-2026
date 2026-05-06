/**
 * @file      ms5611_interface_rp2040.h
 * @brief     MS5611 RP2040 Pico SDK platform interface header
 * @version   1.0.0
 * @date      2026-04-02
 *
 * @details   Platform-specific interface implementation for driving the MS5611
 *            barometric pressure sensor via I2C on an RP2040-based SRAD flight
 *            computer. Implements the callback functions required by the
 *            LibDriver MS5611 library.
 *
 *            Hardware wiring (SRAD 2026 Flight Computer):
 *              - I2C Bus 1: SDA = GPIO2  (RP_SDA1), SCL = GPIO3  (RP_SCL1)
 *                Shared bus with ADXL375. GNSS is on I2C0 (GPIO 12/13).
 *              - MS5611 CSB pin determines I2C address:
 *                  CSB = GND → 0x77 (LibDriver: 0xEE)
 *                  CSB = VDD → 0x76 (LibDriver: 0xEC)
 *
 * @note      This file only implements I2C (not SPI) since the MS5611 is
 *            connected via I2C on the SRAD 2026 PCB. SPI stubs are provided
 *            to satisfy the LibDriver link macros.
 */

#ifndef MS5611_INTERFACE_RP2040_H
#define MS5611_INTERFACE_RP2040_H

#include "driver_ms5611.h"
#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * HARDWARE CONFIGURATION — ADJUST THESE TO MATCH YOUR PCB
 * ========================================================================= */

/**
 * @brief Which I2C peripheral to use.
 *        i2c1 → GPIO 2/3   (RP_SDA1 / RP_SCL1) — MS5611 + ADXL375
 *        i2c0 → GPIO 12/13 (RP_SDA0 / RP_SCL0) — GNSS MAX-M10M only
 *
 *        The MS5611 is on I2C1 on the SRAD 2026 PCB.
 */
#ifndef MS5611_I2C_INSTANCE
#define MS5611_I2C_INSTANCE     i2c1
#endif

/**
 * @brief I2C bus frequency in Hz.
 *        400 kHz (Fast Mode) is recommended and supported by the MS5611.
 */
#ifndef MS5611_I2C_FREQ_HZ
#define MS5611_I2C_FREQ_HZ      (400 * 1000)
#endif

/**
 * @brief GPIO pin numbers for SDA and SCL.
 *        Must match the I2C instance selected above.
 *        i2c1 → SDA=2  (RP_SDA1), SCL=3  (RP_SCL1)
 *        i2c0 → SDA=12 (RP_SDA0), SCL=13 (RP_SCL0)
 */
#ifndef MS5611_I2C_SDA_PIN
#define MS5611_I2C_SDA_PIN      2
#endif

#ifndef MS5611_I2C_SCL_PIN
#define MS5611_I2C_SCL_PIN      3
#endif

/**
 * @brief I2C timeout in microseconds for blocking operations.
 *        50 ms is generous — the longest MS5611 conversion is ~9.04 ms.
 */
#ifndef MS5611_I2C_TIMEOUT_US
#define MS5611_I2C_TIMEOUT_US   (50 * 1000)
#endif

/* =========================================================================
 * INTERFACE FUNCTIONS — plug into LibDriver via LINK macros
 * ========================================================================= */

/**
 * @brief  Initialize the I2C bus for MS5611 communication.
 * @return 0 on success, 1 on failure.
 * @note   Initializes the I2C peripheral, sets GPIO functions, and enables
 *         internal pull-ups. If the bus is already initialized elsewhere
 *         (e.g. shared with BMP280), you may wish to make this a no-op and
 *         handle initialization in main.
 */
uint8_t ms5611_interface_iic_init(void);

/**
 * @brief  Deinitialize the I2C bus.
 * @return 0 on success, 1 on failure.
 * @note   Be careful on a shared bus — deinitializing will affect all devices.
 */
uint8_t ms5611_interface_iic_deinit(void);

/**
 * @brief      Read data from the MS5611 via I2C.
 * @param[in]  addr  8-bit I2C write address (LibDriver convention, e.g. 0xEE).
 * @param[in]  reg   Command/register byte to send before reading.
 * @param[out] buf   Buffer to store the received bytes.
 * @param[in]  len   Number of bytes to read.
 * @return     0 on success, 1 on failure.
 *
 * @note  The MS5611 protocol for I2C reads:
 *        1. Write the command byte (reg) to the device.
 *        2. Read len bytes from the device.
 *        LibDriver passes 8-bit addresses; Pico SDK uses 7-bit internally.
 */
uint8_t ms5611_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief     Write data to the MS5611 via I2C.
 * @param[in] addr  8-bit I2C write address (LibDriver convention).
 * @param[in] reg   Command byte to send.
 * @param[in] buf   Pointer to data bytes to send after the command (can be NULL).
 * @param[in] len   Number of data bytes (can be 0 for command-only writes).
 * @return    0 on success, 1 on failure.
 *
 * @note  For the MS5611, most writes are command-only (len=0): Reset, Convert, etc.
 *        The function sends [reg] followed by [buf[0..len-1]] in a single transaction.
 */
uint8_t ms5611_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief  SPI init stub (not used — MS5611 is on I2C on this board).
 * @return Always 0.
 */
uint8_t ms5611_interface_spi_init(void);

/**
 * @brief  SPI deinit stub.
 * @return Always 0.
 */
uint8_t ms5611_interface_spi_deinit(void);

/**
 * @brief  SPI read stub.
 * @return Always 0.
 */
uint8_t ms5611_interface_spi_read(uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief  SPI write stub.
 * @return Always 0.
 */
uint8_t ms5611_interface_spi_write(uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief     Delay for the specified number of milliseconds.
 * @param[in] ms  Delay time in milliseconds.
 */
void ms5611_interface_delay_ms(uint32_t ms);

/**
 * @brief     Print a debug message via USB/UART stdio.
 * @param[in] fmt  printf-style format string.
 */
void ms5611_interface_debug_print(const char *const fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* MS5611_INTERFACE_RP2040_H */
