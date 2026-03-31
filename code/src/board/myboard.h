// vibe codeado, chequear

#ifndef _BOARDS_MYBOARD_H
#define _BOARDS_MYBOARD_H

// Tell SDK this is RP2040
#define PICO_PLATFORM "rp2040"

// Flash size (change to your flash size)
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)

// Boot stage for common Winbond flash
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

// Optional default pins
#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 0
#define PICO_DEFAULT_UART_RX_PIN 1

// If you have an LED
#define PICO_DEFAULT_LED_PIN 25

#endif