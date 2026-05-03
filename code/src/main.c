#include "pico/stdlib.h"

#define LED_1 26 // blue led

int main() {
    const uint LED_PIN = LED_1;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true) {
        gpio_put(LED_PIN, 1);
        sleep_ms(250);
        gpio_put(LED_PIN, 0);
        sleep_ms(250);
    }
}
