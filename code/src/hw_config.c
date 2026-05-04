#include "hw_config.h"
#include "hardware/pio.h"

static sd_sdio_if_t sdio_if = {
    .CMD_gpio = 18,
    .D0_gpio = 19,

    .SDIO_PIO = pio0,
    .DMA_IRQ_num = DMA_IRQ_0,

    .baud_rate = 125000000 / 12, // ~10 MHz safe start
};

static sd_card_t sd_card = {
    .type = SD_IF_SDIO,
    .sdio_if_p = &sdio_if,
    .use_card_detect = false,
};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    return num == 0 ? &sd_card : NULL;
}
