/**
 * @file      ms5611_main_snippet.c
 * @brief     Example code for initializing and reading the MS5611 barometer.
 * @date      2026-04-02
 *
 * @details   This is NOT a standalone file.  It contains the code that should
 *            go inside your main.c to use the MS5611 driver on the SRAD 2026
 *            flight computer.
 *
 *            Copy the relevant parts into your main when you write it.
 *
 *            Hardware summary (SRAD 2026):
 *              MS5611 on I2C1 (GPIO2 = RP_SDA1, GPIO3 = RP_SCL1)
 *              Shared bus with ADXL375
 *              CSB assumed GND → address 0x77 (LibDriver: MS5611_ADDRESS_CSB_0)
 *
 *            If your CSB is tied to VDD, change MS5611_ADDRESS_CSB_0 to
 *            MS5611_ADDRESS_CSB_1 below.
 */

/* =========================================================================
 * INCLUDES — add these to your main.c
 * ========================================================================= */

#include "pico/stdlib.h"
#include "hardware/i2c.h"

/* LibDriver MS5611 */
#include "driver_ms5611.h"
#include "driver_ms5611_interface.h"
#include "ms5611_interface_rp2040.h"

/* =========================================================================
 * MS5611 HANDLE — declare as a global or file-scope variable
 * ========================================================================= */

static ms5611_handle_t gs_ms5611_handle;

/* =========================================================================
 * INITIALIZATION FUNCTION — call once during startup
 * ========================================================================= */

/**
 * @brief  Set up the MS5611 driver: link platform callbacks, configure
 *         interface/address/OSR, then initialize (reset + read PROM).
 *
 * @return 0 on success, non-zero on error (see ms5611_init return codes).
 *
 * @note   If the I2C1 bus is already initialized (e.g. by the ADXL375 driver),
 *         that's fine — ms5611_interface_iic_init() detects this and skips
 *         re-initialization.  Just make sure the bus is up before calling
 *         this function.
 */
uint8_t ms5611_setup(void)
{
    uint8_t res;

    /* --- Step 1: zero out the handle ----------------------------------- */
    DRIVER_MS5611_LINK_INIT(&gs_ms5611_handle, ms5611_handle_t);

    /* --- Step 2: link all platform callbacks --------------------------- */
    DRIVER_MS5611_LINK_IIC_INIT(&gs_ms5611_handle,    ms5611_interface_iic_init);
    DRIVER_MS5611_LINK_IIC_DEINIT(&gs_ms5611_handle,  ms5611_interface_iic_deinit);
    DRIVER_MS5611_LINK_IIC_READ(&gs_ms5611_handle,    ms5611_interface_iic_read);
    DRIVER_MS5611_LINK_IIC_WRITE(&gs_ms5611_handle,   ms5611_interface_iic_write);
    DRIVER_MS5611_LINK_SPI_INIT(&gs_ms5611_handle,    ms5611_interface_spi_init);
    DRIVER_MS5611_LINK_SPI_DEINIT(&gs_ms5611_handle,  ms5611_interface_spi_deinit);
    DRIVER_MS5611_LINK_SPI_READ(&gs_ms5611_handle,    ms5611_interface_spi_read);
    DRIVER_MS5611_LINK_SPI_WRITE(&gs_ms5611_handle,   ms5611_interface_spi_write);
    DRIVER_MS5611_LINK_DELAY_MS(&gs_ms5611_handle,    ms5611_interface_delay_ms);
    DRIVER_MS5611_LINK_DEBUG_PRINT(&gs_ms5611_handle, ms5611_interface_debug_print);

    /* --- Step 3: configure interface and address ----------------------- */
    res = ms5611_set_interface(&gs_ms5611_handle, MS5611_INTERFACE_IIC);
    if (res != 0)
    {
        printf("ms5611: set_interface failed (%u)\n", res);
        return res;
    }

    /*
     * MS5611_ADDRESS_CSB_0 = CSB tied to GND → I2C address 0x77
     * MS5611_ADDRESS_CSB_1 = CSB tied to VDD → I2C address 0x76
     *
     * Check your KiCad schematic to see where the CSB net goes.
     * If unsure, try CSB_0 first — it's the most common wiring.
     */
    res = ms5611_set_addr_pin(&gs_ms5611_handle, MS5611_ADDRESS_CSB_0);
    if (res != 0)
    {
        printf("ms5611: set_addr_pin failed (%u)\n", res);
        return res;
    }

    /* --- Step 4: set oversampling ratio -------------------------------- */
    /*
     * OSR tradeoff:
     *   OSR_256  → fastest (~0.6 ms conversion), lowest resolution
     *   OSR_4096 → slowest (~9.04 ms conversion), highest resolution
     *
     * For a flight computer reading at ~10 Hz, OSR_4096 is fine because
     * the total read cycle (temp + pressure) takes ~20 ms, leaving plenty
     * of room in a 100 ms period.  For faster loops, consider OSR_1024.
     */
    res = ms5611_set_temperature_osr(&gs_ms5611_handle, MS5611_OSR_4096);
    if (res != 0)
    {
        printf("ms5611: set_temperature_osr failed (%u)\n", res);
        return res;
    }

    res = ms5611_set_pressure_osr(&gs_ms5611_handle, MS5611_OSR_4096);
    if (res != 0)
    {
        printf("ms5611: set_pressure_osr failed (%u)\n", res);
        return res;
    }

    /* --- Step 5: initialize (reset chip + read PROM calibration) ------- */
    res = ms5611_init(&gs_ms5611_handle);
    if (res != 0)
    {
        printf("ms5611: init failed (%u)\n", res);
        /*
         * Error codes:
         *   1 = I2C init failed
         *   2 = handle NULL
         *   3 = a linked function is NULL
         *   4 = reset command failed (check wiring / address / pull-ups)
         *   5 = PROM read failed or CRC mismatch
         */
        return res;
    }

    printf("ms5611: initialized OK on I2C1 (GPIO2/GPIO3)\n");
    return 0;
}

/* =========================================================================
 * READ FUNCTION — call from your acquisition loop
 * ========================================================================= */

/**
 * @brief  Read compensated temperature and pressure from the MS5611.
 *
 * @param[out] temperature_c   Temperature in degrees Celsius.
 * @param[out] pressure_mbar   Pressure in millibar (hPa).
 * @return     0 on success, non-zero on error.
 *
 * @note   This call is BLOCKING — it waits for both the temperature and
 *         pressure ADC conversions to complete (up to ~20 ms at OSR 4096).
 *         Plan your acquisition loop timing accordingly.
 */
uint8_t ms5611_read(float *temperature_c, float *pressure_mbar)
{
    uint32_t temp_raw;
    uint32_t press_raw;

    return ms5611_read_temperature_pressure(&gs_ms5611_handle,
                                            &temp_raw, temperature_c,
                                            &press_raw, pressure_mbar);
}

/* =========================================================================
 * SHUTDOWN — call when done (optional)
 * ========================================================================= */

/**
 * @brief  Deinitialize the MS5611 driver and release the I2C bus
 *         (only if no other device is using it).
 */
uint8_t ms5611_shutdown(void)
{
    return ms5611_deinit(&gs_ms5611_handle);
}

/* =========================================================================
 * EXAMPLE MAIN — for reference only, adapt to your real main.c
 * ========================================================================= */

#if 0   /* Change to #if 1 to compile as a standalone test program */

int main(void)
{
    /* --- Platform init ------------------------------------------------- */
    stdio_init_all();
    sleep_ms(2000);               /* wait for USB serial to enumerate */
    printf("SRAD 2026 — MS5611 barometer test\n");

    /* --- Sensor init --------------------------------------------------- */
    if (ms5611_setup() != 0)
    {
        printf("FATAL: MS5611 initialization failed.\n");
        while (1) { tight_loop_contents(); }
    }

    /* --- Acquisition loop ---------------------------------------------- */
    while (1)
    {
        float temperature_c;
        float pressure_mbar;

        if (ms5611_read(&temperature_c, &pressure_mbar) == 0)
        {
            printf("T = %.2f C  |  P = %.2f mbar\n",
                   temperature_c, pressure_mbar);
        }
        else
        {
            printf("ms5611: read error\n");
        }

        sleep_ms(100);            /* ~10 Hz read rate */
    }

    /* never reached */
    ms5611_shutdown();
    return 0;
}

#endif  /* standalone test */
