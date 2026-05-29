#ifndef FRAME_RING_BUFFER_H
#define FRAME_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "adxl375.h"
#include "iim20670.h"
#include "ms5611.h"
#include "max_m10s.h"

#define FRAME_RING_SIZE 2048 // rp2040 has 256 KB of RAM

typedef struct __attribute__((packed)) {
	uint16_t sync_word;

	uint64_t timestamp_us;
	uint32_t frame_number;

	iim_sample_t imu;
	adxl375_sample_t adxl;
	ms5611_sample_t baro;
	uint16_t battery_mv;
	gnss_sample_t gnss;

	uint8_t freshness;

	uint16_t crc16;
} sensor_frame_t; // a sensor frame instance has 139 bytes

typedef struct {
	volatile uint32_t head;
	volatile uint32_t tail;
	volatile uint32_t dropped;
	sensor_frame_t buffer[FRAME_RING_SIZE];
} frame_ring_t;

bool frame_ring_push(frame_ring_t *ring, const sensor_frame_t *frame);
bool frame_ring_pop(frame_ring_t *ring, sensor_frame_t *frame);

#endif
