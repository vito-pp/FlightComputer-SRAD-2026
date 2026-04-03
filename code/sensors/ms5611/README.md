# MS5611 Barometer Driver — SRAD 2026 Flight Computer

Driver for the MS5611 barometric pressure sensor connected via **I2C1** (GPIO2/GPIO3)
on the SRAD 2026 RP2040 flight computer.

## File structure

```
ms5611/
├── driver_ms5611.h                 ← LibDriver header (unmodified)
├── driver_ms5611.c                 ← LibDriver source (PATCHED — see below)
├── driver_ms5611_interface.h       ← LibDriver interface declarations (unmodified)
├── ms5611_interface_rp2040.h       ← Platform config: I2C bus, pins, timeouts
├── ms5611_interface_rp2040.c       ← Platform implementation: Pico SDK I2C callbacks
├── ms5611_main_snippet.c           ← Setup/read/shutdown functions + example main
├── CMakeLists_fragment.cmake       ← Copy into your build system
└── README.md                       ← This file
```

## Bug fix applied to LibDriver

The original LibDriver (`driver_ms5611.c`) uses **MS5607** second-order temperature
compensation coefficients, which are **wrong for the MS5611**.  This has been corrected
in the patched copy.

| Coefficient         | LibDriver (MS5607, wrong) | Corrected (MS5611 datasheet) |
|---------------------|---------------------------|------------------------------|
| T2 (TEMP < 20°C)   | 3·dT²/2³³                | dT²/2³¹                     |
| OFF2                | 61·(T-2000)²/16          | 5·(T-2000)²/2               |
| SENS2               | 29·(T-2000)²/16          | 5·(T-2000)²/4               |
| OFF2 (T < -15°C)   | +17·(T+1500)²            | +7·(T+1500)²                |
| SENS2 (T < -15°C)  | +9·(T+1500)²             | +11·(T+1500)²/2             |
| T2 (TEMP >= 20°C)  | 5·dT²/2³⁸               | 0                            |

The wrong coefficients would produce significant pressure errors at low temperatures
(exactly the conditions during high-altitude flight).

## Hardware configuration

Default settings in `ms5611_interface_rp2040.h`:

| Parameter        | Default   | Notes                                        |
|------------------|-----------|----------------------------------------------|
| I2C instance     | `i2c1`   | Shared with ADXL375                          |
| SDA pin          | GPIO 2    | RP_SDA1                                     |
| SCL pin          | GPIO 3    | RP_SCL1                                     |
| I2C frequency    | 400 kHz   | Fast Mode                                   |
| I2C address      | 0x77      | CSB = GND (change in ms5611_main_snippet.c) |
| Timeout          | 50 ms     | Generous for worst-case conversion           |

To change the I2C address (if CSB is tied to VDD on your PCB), edit this line
in `ms5611_main_snippet.c`:

```c
ms5611_set_addr_pin(&gs_ms5611_handle, MS5611_ADDRESS_CSB_1);  /* 0x76 */
```

## How to integrate into your project

1. Copy the entire `ms5611/` folder into your project tree (e.g. `drivers/ms5611/`).

2. Add to your `CMakeLists.txt`:

```cmake
add_library(ms5611
    drivers/ms5611/driver_ms5611.c
    drivers/ms5611/ms5611_interface_rp2040.c
)

target_include_directories(ms5611 PUBLIC
    drivers/ms5611
)

target_link_libraries(ms5611
    pico_stdlib
    hardware_i2c
    hardware_gpio
)

# Then link to your main target:
target_link_libraries(your_main_target ms5611)
```

3. In your `main.c`, include and call:

```c
#include "ms5611_main_snippet.c"   /* or copy the functions directly */

int main(void) {
    stdio_init_all();

    /* Initialize I2C1 bus (if not done by another driver) */
    /* ... */

    /* Initialize MS5611 */
    if (ms5611_setup() != 0) {
        /* handle error */
    }

    /* In your loop */
    float temp, press;
    ms5611_read(&temp, &press);
}
```

## Shared I2C1 bus notes

The ADXL375 and MS5611 share I2C1.  The interface layer handles this:

- `ms5611_interface_iic_init()` checks if the bus is already configured.
  If yes, it skips initialization (avoids double-init).
- `ms5611_interface_iic_deinit()` only deinitializes if it was the one that
  set up the bus.

**Recommendation:** initialize I2C1 once in your platform init code (before
any sensor driver), and let both drivers detect that the bus is already up.

## OSR selection guide

| OSR   | Conversion time | Resolution   | Suggested use              |
|-------|-----------------|-------------|----------------------------|
| 256   | ~0.6 ms         | lowest      | Fast loops (>50 Hz)        |
| 512   | ~1.2 ms         | low         |                            |
| 1024  | ~2.3 ms         | medium      | Balanced (25–50 Hz)        |
| 2048  | ~4.5 ms         | high        |                            |
| 4096  | ~9.0 ms         | highest     | Accuracy priority (~10 Hz) |

At OSR 4096, a full temp+pressure read takes ~20 ms (blocking).  At 10 Hz
acquisition rate, this leaves 80 ms per cycle for other tasks.
