#ifndef XBEE_H
#define XBEE_H

#include <stddef.h>
#include <stdint.h>
#include "pico/types.h"
#include "hardware/uart.h"

/*
 * XBee-PRO 900HP/S3B UART driver for RP2040 / Pico SDK.
 *
 * This driver only transmits. It assumes the XBee is already configured
 * externally, for example with XCTU, for the same UART settings used here.
 * For raw byte payloads, keep the XBee in Transparent mode (AP = 0), unless
 * you intentionally add API frame generation above this driver.
 */

#define XBEE_UART_ID uart0
#define XBEE_UART_TX_GPIO 16
#define XBEE_UART_RX_GPIO 17
#define XBEE_UART_BAUDRATE 115200
#define XBEE_UART_DATA_BITS 8u
#define XBEE_UART_STOP_BITS 1u
#define XBEE_UART_PARITY UART_PARITY_NONE
/* Optional hardware flow control. Leave disabled unless CTS/RTS are wired. */
#define XBEE_UART_ENABLE_CTS false
#define XBEE_UART_ENABLE_RTS false

void xbee_init(void);
void xbee_transmit(const void *msg, size_t length);

#endif /* XBEE_H */
