#ifndef CRC_H
#define CRC_H

#include <stdint.h>

#include "frame_ring_buffer.h"

uint16_t crc16_ccitt_false(const sensor_frame_t *frame);

#endif
