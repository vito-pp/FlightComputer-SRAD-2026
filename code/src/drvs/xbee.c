#include "xbee.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

void xbee_init(void)
{
    uart_init(XBEE_UART_ID, XBEE_UART_BAUDRATE);

    gpio_set_function(XBEE_UART_TX_GPIO, GPIO_FUNC_UART);
    gpio_set_function(XBEE_UART_RX_GPIO, GPIO_FUNC_UART);

    uart_set_format(XBEE_UART_ID,
                    XBEE_UART_DATA_BITS,
                    XBEE_UART_STOP_BITS,
                    XBEE_UART_PARITY);

    uart_set_hw_flow(XBEE_UART_ID,
                     XBEE_UART_ENABLE_CTS,
                     XBEE_UART_ENABLE_RTS);

    /* This is a TX-only driver from the application point of view. */
    uart_set_fifo_enabled(XBEE_UART_ID, true);
}

void xbee_transmit(const void *msg, size_t length)
{
    if (msg == NULL || length == 0u) {
        return;
    }

    uart_write_blocking(XBEE_UART_ID, (const uint8_t *)msg, length);
}
