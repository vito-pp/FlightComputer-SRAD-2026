#include "drvs/buzzer.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define BUZZER_GPIO 0

static uint buzzer_slice;

void buzz_init(void)
{
    gpio_set_function(BUZZER_GPIO, GPIO_FUNC_PWM);

    buzzer_slice = pwm_gpio_to_slice_num(BUZZER_GPIO);

    pwm_config cfg = pwm_get_default_config();

    /*
     * PWM frequency:
     *
     * f = 125MHz / clkdiv / (wrap + 1)
     *
     * Using:
     * clkdiv = 125
     * wrap   = 249
     *
     * -> 4kHz
     */

    pwm_config_set_clkdiv(&cfg, 125.0f);

    pwm_init(buzzer_slice, &cfg, false);

    pwm_set_wrap(buzzer_slice, 249);

    // 50% duty cycle
    pwm_set_gpio_level(BUZZER_GPIO, 125);

    pwm_set_enabled(buzzer_slice, false);
}

void buzz_alarm(bool enable)
{
    pwm_set_enabled(buzzer_slice, enable);
}
