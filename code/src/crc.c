#include "crc.h"

#include <stddef.h>

uint16_t crc16_ccitt_false(const sensor_frame_t *frame)
{
	const uint8_t *bytes = (const uint8_t *)frame;
	const size_t start = offsetof(sensor_frame_t, timestamp_us);
	const size_t end = offsetof(sensor_frame_t, crc16);
	uint16_t crc = 0xFFFF;

	for (size_t i = start; i < end; ++i) {
		crc ^= (uint16_t)bytes[i] << 8;

		for (uint8_t bit = 0; bit < 8; ++bit) {
			if ((crc & 0x8000u) != 0u) {
				crc = (crc << 1) ^ 0x1021u;
			}
			else {
				crc <<= 1;
			}
		}
	}

	return crc;
}
