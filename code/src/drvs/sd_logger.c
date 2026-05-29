#include "sd_logger.h"
// #include "pico/stdio.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ff.h"

static FATFS fs;
static bool mounted = false;
static bool csv_header_checked = false;

static bool sd_logger_write_csv_line(const char *line) {
	if (!mounted) return false;

	FIL file;
	FRESULT fr;
	UINT bw;
	size_t len = strlen(line);

	fr = f_open(&file, "0:/log.csv", FA_WRITE | FA_OPEN_APPEND);
	if (fr != FR_OK) {
		// printf("CSV open failed: %d\n", fr);
		// stdio_flush();
		return false;
	}

	fr = f_write(&file, line, len, &bw);
	if (fr != FR_OK || bw != len) {
		// printf("CSV write failed: %d\n", fr);
		// stdio_flush();
		f_close(&file);
		return false;
	}

	f_sync(&file);
	f_close(&file);

	return true;
}

static bool sd_logger_ensure_csv_header(void) {
	if (csv_header_checked) return true;

	FILINFO info;
	FRESULT fr = f_stat("0:/log.csv", &info);

	if (fr == FR_NO_FILE || (fr == FR_OK && info.fsize == 0)) {
		if (!sd_logger_write_csv_line(
			"sync_word,frame_number,timestamp_us,"
			"imu_ax_g,imu_ay_g,imu_az_g,"
			"imu_gx_dps,imu_gy_dps,imu_gz_dps,"
			"imu_temp1_c,imu_temp2_c,imu_error_count,"
			"adxl_ax_g,adxl_ay_g,adxl_az_g,"
			"pressure_mbar,temperature_c,altitude_m,"
			"battery_mv,"
			"gnss_itow_ms,gnss_lon_deg_e7,gnss_lat_deg_e7,"
			"gnss_height_mm,gnss_hmsl_mm,"
			"gnss_veln_mm_s,gnss_vele_mm_s,gnss_veld_mm_s,"
			"gnss_gspeed_mm_s,"
			"gnss_hacc_mm,gnss_vacc_mm,gnss_sacc_mm_s,"
			"gnss_fixtype,gnss_numsv,gnss_flags,gnss_reserved,"
			"freshness,crc16\n")) {
			return false;
		}
	}
	else if (fr != FR_OK) {
		// printf("CSV stat failed: %d\n", fr);
		// stdio_flush();
		return false;
	}

	csv_header_checked = true;
	return true;
}

bool sd_logger_init(void) {
	FRESULT fr = f_mount(&fs, "0:", 1);

	if (fr != FR_OK) {
		// printf("SD mount failed: %d\n", fr);
		// stdio_flush();
		return false;
	}

	mounted = true;
	// printf("SD mounted OK\n");
	// stdio_flush();
	return true;
}

bool sd_logger_append_csv(const char *line) {
	return sd_logger_write_csv_line(line);
}

bool sd_logger_append_frame_csv(const sensor_frame_t *frame) {
	if (!mounted) return false;
	if (!sd_logger_ensure_csv_header()) return false;

	char line[512];
	int len = snprintf(
		line,
		sizeof(line),
		"0x%04X,%lu,%llu,"
		"%.6f,%.6f,%.6f,"
		"%.6f,%.6f,%.6f,"
		"%.6f,%.6f,%lu,"
		"%.6f,%.6f,%.6f,"
		"%.6f,%.6f,%.6f,"
		"%u,"
		"%lu,%ld,%ld,"
		"%ld,%ld,"
		"%ld,%ld,%ld,"
		"%ld,"
		"%lu,%lu,%lu,"
		"%u,%u,0x%02X,%u,"
		"0x%02X,0x%04X\n",
		frame->sync_word,
		(unsigned long)frame->frame_number,
		(unsigned long long)frame->timestamp_us,
		frame->imu.accel_x_g,
		frame->imu.accel_y_g,
		frame->imu.accel_z_g,
		frame->imu.gyro_x_dps,
		frame->imu.gyro_y_dps,
		frame->imu.gyro_z_dps,
		frame->imu.temp1_c,
		frame->imu.temp2_c,
		(unsigned long)frame->imu.error_count,
		frame->adxl.x_g,
		frame->adxl.y_g,
		frame->adxl.z_g,
		frame->baro.pressure_mbar,
		frame->baro.temperature_c,
		frame->baro.altitude_m,
		frame->battery_mv,
		(unsigned long)frame->gnss.iTOW_ms,
		(long)frame->gnss.lon_deg_e7,
		(long)frame->gnss.lat_deg_e7,
		(long)frame->gnss.height_mm,
		(long)frame->gnss.hMSL_mm,
		(long)frame->gnss.velN_mm_s,
		(long)frame->gnss.velE_mm_s,
		(long)frame->gnss.velD_mm_s,
		(long)frame->gnss.gSpeed_mm_s,
		(unsigned long)frame->gnss.hAcc_mm,
		(unsigned long)frame->gnss.vAcc_mm,
		(unsigned long)frame->gnss.sAcc_mm_s,
		frame->gnss.fixType,
		frame->gnss.numSV,
		frame->gnss.flags,
		frame->gnss.reserved,
		frame->freshness,
		frame->crc16
	);

	if (len < 0 || (size_t)len >= sizeof(line)) {
		// printf("CSV frame format failed\n");
		// stdio_flush();
		return false;
	}

	return sd_logger_write_csv_line(line);
}

bool sd_logger_increment_counter(int *new_value) {
	if (!mounted) return false;

	FIL file;
	FRESULT fr;
	UINT br, bw;

	char buffer[64] = {0};
	int counter = 0;

	fr = f_open(&file, "0:/counter.txt", FA_READ);
	if (fr == FR_OK) {
		fr = f_read(&file, buffer, sizeof(buffer) - 1, &br);
		f_close(&file);

		if (fr != FR_OK) return false;

		buffer[br] = '\0';
		counter = atoi(buffer);
	}

	counter++;

	char out[64];
	snprintf(out, sizeof(out), "%d\n", counter);

	fr = f_open(&file, "0:/counter.txt", FA_WRITE | FA_CREATE_ALWAYS);
	if (fr != FR_OK) return false;

	fr = f_write(&file, out, strlen(out), &bw);
	if (fr != FR_OK || bw != strlen(out)) {
		f_close(&file);
		return false;
	}

	f_sync(&file);
	f_close(&file);

	if (new_value) {
		*new_value = counter;
	}

	return true;
}
