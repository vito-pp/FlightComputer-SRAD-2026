#ifndef MAX_M10S_H
#define MAX_M10S_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

/* User configuration */
#define MAX_M10S_I2C_PORT          i2c0
#define MAX_M10S_I2C_SDA_PIN       12
#define MAX_M10S_I2C_SCL_PIN       13
#define MAX_M10S_I2C_BAUDRATE_HZ   400000

/* u-blox 8-bit I2C address is 0x84. Pico SDK uses 7-bit address. */
#define MAX_M10S_I2C_ADDR          0x42

#define MAX_M10S_READ_CHUNK_SIZE   64
#define MAX_M10S_I2C_TIMEOUT_US    2000

/**
 * @brief Result codes returned by max_m10s_poll_sample().
 */
typedef enum {
	MAX_M10S_POLL_OK = 0,    /**< New sample was read. */
	MAX_M10S_POLL_NO_DATA,   /**< No complete UBX-NAV-PVT sample is available */
	MAX_M10S_POLL_ERROR      /**< Invalid parameter or I2C read failure. */
} max_m10s_poll_status_t;

typedef struct __attribute__((packed)) {

	/** GPS time of week in milliseconds. Resets every GPS week. */
	uint32_t iTOW_ms;

	/** Longitude in degrees * 1e-7. */
	int32_t lon_deg_e7;

	/** Latitude in degrees * 1e-7. */
	int32_t lat_deg_e7;

	/** Height above WGS84 ellipsoid in millimeters. */
	int32_t height_mm;

	/** Height above mean sea level in millimeters. */
	int32_t hMSL_mm;

	/** Velocity toward geographic north in mm/s. */
	int32_t velN_mm_s;

	/** Velocity toward geographic east in mm/s. */
	int32_t velE_mm_s;

	/** Velocity toward geographic down in mm/s. Negative means climbing. */
	int32_t velD_mm_s;

	/** Ground speed magnitude in mm/s. */
	int32_t gSpeed_mm_s;

	/** Horizontal position accuracy estimate in mm. */
	uint32_t hAcc_mm;

	/** Vertical position accuracy estimate in mm. */
	uint32_t vAcc_mm;

	/** Speed accuracy estimate in mm/s. */
	uint32_t sAcc_mm_s;

	/** GNSS fix type. */
	uint8_t fixType;

	/** Number of satellites used in solution. */
	uint8_t numSV;

	/** Status flags from UBX-NAV-PVT. */
	uint8_t flags;

} gnss_sample_t;

/**
 * @brief Initializes the MAX-M10S I2C interface.
 *
 * This initializes only the RP2040 I2C peripheral and GPIO pins. It does not
 * configure the GNSS receiver output messages.
 *
 * @return true if the receiver ACKs on the I2C bus.
 * @return false if the receiver does not respond.
 */
bool max_m10s_init(void);

/**
 * @brief Polls the MAX-M10S I2C stream and parses one GNSS sample.
 *
 * This function performs one bounded non-blocking-style polling step. It checks
 * how many bytes are available, drains at most MAX_M10S_READ_CHUNK_SIZE bytes,
 * and feeds them into the UBX parser.
 *
 * @param[out] sample Destination for the parsed sample.
 *
 * @return MAX_M10S_POLL_OK if a new UBX-NAV-PVT sample was parsed.
 * @return MAX_M10S_POLL_NO_DATA if no complete sample is available yet.
 * @return MAX_M10S_POLL_ERROR if sample is NULL or an I2C transaction failed.
 */
max_m10s_poll_status_t max_m10s_poll_sample(gnss_sample_t *sample);

#endif /* MAX_M10S_H */
