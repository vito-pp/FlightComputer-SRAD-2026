#include "drvs/adc_bat_lvl.h"

#include <stdbool.h>

#include "hardware/adc.h"
#include "hardware/structs/adc.h"

#define BAT_ADC_GPIO 28
#define BAT_ADC_INPUT 2

#define ADC_MAX_COUNT 4095u
#define ADC_REF_MV 3300u

#define BAT_DIVIDER_TOP_KOHM 20u
#define BAT_DIVIDER_BOTTOM_KOHM 30u
#define BAT_DIVIDER_TOTAL_KOHM (BAT_DIVIDER_TOP_KOHM + BAT_DIVIDER_BOTTOM_KOHM)

static bool conversion_active;

static void start_conversion(void)
{
	adc_select_input(BAT_ADC_INPUT);
	adc_hw->cs |= ADC_CS_START_ONCE_BITS;
	conversion_active = true;
}

static uint16_t raw_to_battery_mv(uint16_t raw)
{
	const uint32_t numerator = (uint32_t)raw * ADC_REF_MV * BAT_DIVIDER_TOTAL_KOHM;
	const uint32_t denominator = ADC_MAX_COUNT * BAT_DIVIDER_BOTTOM_KOHM;

	return (uint16_t)((numerator + denominator / 2u) / denominator);
}

void adc_bat_lvl_init(void)
{
	adc_init();
	adc_gpio_init(BAT_ADC_GPIO);
	start_conversion();
}

bat_lvl_poll_status_t adc_bat_lvl_poll(uint16_t *battery_mv)
{
	if (battery_mv == NULL) {
		return BAT_LVL_POLL_ERROR;
	}

	if (!conversion_active) {
		start_conversion();
		return BAT_LVL_POLL_NO_DATA;
	}

	if ((adc_hw->cs & ADC_CS_READY_BITS) == 0u) {
		return BAT_LVL_POLL_NO_DATA;
	}

	*battery_mv = raw_to_battery_mv((uint16_t)adc_hw->result);
	conversion_active = false;
	start_conversion();

	return BAT_LVL_POLL_OK;
}
