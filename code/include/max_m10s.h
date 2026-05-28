#ifndef MAX_M10S_H
#define MAX_M10S_H

#include <stdint.h>

typedef struct __attribute__((packed)) {

	// GPS time of week in milliseconds
	// Resets every GPS week
	uint32_t iTOW_ms;

	// Longitude in degrees * 1e-7
	// Example:
	// -58.3815923 deg -> -583815923
	int32_t lon_deg_e7;

	// Latitude in degrees * 1e-7
	// Example:
	// -34.603722 deg -> -346037220
	int32_t lat_deg_e7;

	// Height above WGS84 ellipsoid in millimeters
	// This is the raw GNSS altitude
	int32_t height_mm;

	// Height above mean sea level in millimeters
	// Usually more useful for humans/maps
	int32_t hMSL_mm;

	// Velocity toward geographic north in mm/s
	int32_t velN_mm_s;

	// Velocity toward geographic east in mm/s
	int32_t velE_mm_s;

	// Velocity toward geographic down in mm/s
	// Negative means climbing
	int32_t velD_mm_s;

	// Ground speed magnitude in mm/s
	// 2D horizontal speed
	int32_t gSpeed_mm_s;

	// Horizontal position accuracy estimate in mm
	uint32_t hAcc_mm;

	// Vertical position accuracy estimate in mm
	uint32_t vAcc_mm;

	// Speed accuracy estimate in mm/s
	uint32_t sAcc_mm_s;

	// GNSS fix type
	// 0 = no fix
	// 1 = dead reckoning only
	// 2 = 2D fix
	// 3 = 3D fix
	// 4 = GNSS + dead reckoning
	// 5 = time-only fix
	uint8_t fixType;

	// Number of satellites used in solution
	uint8_t numSV;

	// Status flags from UBX NAV-PVT
	// bit0: gnssFixOK
	// bit1: differential solution
	// bit6-7: carrier solution status
	uint8_t flags;

} gnss_sample_t;

#endif // MAX_M10S_H
