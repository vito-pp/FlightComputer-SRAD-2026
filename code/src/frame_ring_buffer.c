#include "frame_ring_buffer.h"
#include "hardware/sync.h"

bool frame_ring_push(frame_ring_t *ring, const sensor_frame_t *frame)
{
	uint32_t head = ring->head;
	uint32_t next_head = (head + 1u) % FRAME_RING_SIZE;

	if (next_head == ring->tail) {
		ring->dropped++;
		return false;
	}

	ring->buffer[head] = *frame;
	__dmb(); // ensure frame is written before publishing head
	ring->head = next_head;
	return true;
}

bool frame_ring_pop(frame_ring_t *ring, sensor_frame_t *frame)
{
	uint32_t tail = ring->tail;

	if (tail == ring->head) {
		return false;
	}

	*frame = ring->buffer[tail];
	__dmb(); // ensure frame is read before publishing tail
	ring->tail = (tail + 1u) % FRAME_RING_SIZE;
	return true;
}
