#ifndef ADC_BAT_LVL_H
#define ADC_BAT_LVL_H

#include <stdint.h>

typedef enum {
	BAT_LVL_POLL_OK = 0,
	BAT_LVL_POLL_NO_DATA,
	BAT_LVL_POLL_ERROR
} bat_lvl_poll_status_t;

/**
 * @brief Initializes ADC2 on GPIO28 for battery voltage measurement.
 */
void adc_bat_lvl_init(void);

/**
 * @brief Polls one nonblocking battery ADC conversion.
 *
 * If no conversion is active, this starts one and returns
 * BAT_LVL_POLL_NO_DATA. Once the ADC result is ready, it stores the
 * reconstructed battery voltage in millivolts, starts the next conversion,
 * and returns BAT_LVL_POLL_OK.
 *
 * The ADC input is assumed to be behind a resistor divider with gain
 * 30 / (20 + 30), so the measured ADC voltage is scaled back up by 5 / 3.
 */
bat_lvl_poll_status_t adc_bat_lvl_poll(uint16_t *battery_mv);

#endif
