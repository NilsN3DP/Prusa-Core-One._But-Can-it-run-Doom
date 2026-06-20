#include "w25q.h"
#include "spi.h"
#include "board.h"
#include "gpio.h"

#define CMD_JEDEC_ID 0x9F
#define CMD_READ     0x03

static inline void cs_low(void)  { gpio_clear(FLASH_CS_PORT, FLASH_CS_PIN); }
static inline void cs_high(void) { gpio_set(FLASH_CS_PORT, FLASH_CS_PIN); }

void w25q_read_jedec_id(uint8_t id[3]) {
    cs_low();
    spi_xfer(FLASH_SPI, CMD_JEDEC_ID);
    id[0] = spi_xfer(FLASH_SPI, 0xFF);
    id[1] = spi_xfer(FLASH_SPI, 0xFF);
    id[2] = spi_xfer(FLASH_SPI, 0xFF);
    cs_high();
}

void w25q_read(uint32_t addr, uint8_t *buf, size_t n) {
    cs_low();
    spi_xfer(FLASH_SPI, CMD_READ);
    spi_xfer(FLASH_SPI, (uint8_t)(addr >> 16));
    spi_xfer(FLASH_SPI, (uint8_t)(addr >> 8));
    spi_xfer(FLASH_SPI, (uint8_t)(addr));
    spi_read(FLASH_SPI, buf, n);
    cs_high();
}
