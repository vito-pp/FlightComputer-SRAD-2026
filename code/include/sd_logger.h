#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>

#include "frame_ring_buffer.h"

/**
 * @file sd_logger.h
 * @brief Simple SD-card logging interface for the RP2040 flight computer.
 *
 * This module wraps the FatFS-based SD-card library and provides a small,
 * application-level API for mounting the SD card, appending CSV lines, and
 * testing persistent storage with a boot counter.
 */

/**
 * @brief Mount and initialize the SD card filesystem.
 *
 * This function must be called once before using any other SD logger function.
 * Internally, it mounts the FatFS volume at drive `"0:"`.
 *
 * @return true if the SD card was mounted successfully.
 * @return false if mounting failed.
 */
bool sd_logger_init(void);

/**
 * @brief Append one line of text to the CSV log file.
 *
 * The line is appended to `0:/log.csv`. The caller is responsible for including
 * the trailing newline character (`\n`) if a new CSV row is desired.
 *
 * Example:
 * @code
 * sd_logger_append_csv("1234,24.5,101325,0.01\n");
 * @endcode
 *
 * @param line Null-terminated string to append to the CSV file.
 *
 * @return true if the line was written successfully.
 * @return false if the SD card is not mounted or the write operation failed.
 */
bool sd_logger_append_csv(const char *line);

/**
 * @brief Append a sensor frame to the CSV log file.
 *
 * The first call writes a header row if `0:/log.csv` is empty or does not
 * exist. Columns are written in this order:
 * sync_word, frame_number, timestamp_us, IMU, ADXL, barometer, battery, GNSS,
 * freshness, and crc16 fields.
 *
 * @param frame Sensor frame to serialize.
 *
 * @return true if the row was written successfully.
 * @return false if the SD card is not mounted or the write operation failed.
 */
bool sd_logger_append_frame_csv(const sensor_frame_t *frame);

/**
 * @brief Increment and persist a counter stored on the SD card.
 *
 * This function reads `0:/counter.txt`, parses the previous integer value,
 * increments it by one, writes the new value back to the file, and optionally
 * returns the new value through `new_value`.
 *
 * This is useful as a persistence test: if the value increases after power
 * cycling the board, then SD writes are surviving disconnects/reboots.
 *
 * @param[out] new_value Optional pointer where the updated counter value will
 * be stored. Pass `NULL` if the value is not needed.
 *
 * @return true if the counter was read/written successfully.
 * @return false if the SD card is not mounted or the operation failed.
 */
bool sd_logger_increment_counter(int *new_value);

#endif
