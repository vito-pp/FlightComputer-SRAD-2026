#ifndef FRAME_RING_BUFFER_H
#define FRAME_RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

#include "adxl375.h"
#include "iim20670.h"
#include "ms5611.h"

#define FRAME_RING_SIZE 512

typedef struct {
	uint64_t timestamp_us;

	iim_sample_t imu;
	adxl375_sample_t adxl;
	ms5611_sample_t baro;

	uint8_t freshness;
} sensor_frame_t;

typedef struct {
	volatile uint32_t head;
	volatile uint32_t tail;
	volatile uint32_t dropped;
	sensor_frame_t buffer[FRAME_RING_SIZE];
} frame_ring_t;

bool frame_ring_push(frame_ring_t *ring, const sensor_frame_t *frame);
bool frame_ring_pop(frame_ring_t *ring, sensor_frame_t *frame);

#endif
