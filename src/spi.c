#include "spi.h"
#include "board.h"
#include "gpio.h"

static void spi_setup(SPI_TypeDef *spi, int div_code) {
    /* Master, 8-bit, mode 0 (CPOL=0, CPHA=0), MSB-first, software NSS. */
    spi->CR1 = 0;
    spi->CR1 = ((uint32_t)div_code << 3)
             | SPI_CR1_MSTR
             | SPI_CR1_SSM | SPI_CR1_SSI;
    spi->CR2 = 0;
    spi->CR1 |= SPI_CR1_SPE;
}

void spi_set_baud(SPI_TypeDef *spi, int div_code) {
    spi->CR1 &= ~SPI_CR1_SPE;
    spi->CR1 = (spi->CR1 & ~(7u << 3)) | ((uint32_t)div_code << 3);
    spi->CR1 |= SPI_CR1_SPE;
}

void spi_init_pins_and_periph(void) {
    /* ---- SPI6 (display): SCK PG13, MISO PG12, MOSI PG14, AF5 ---- */
    gpio_config(DISP_SCK_PORT,  DISP_SCK_PIN,  GPIO_AF, GPIO_PP, GPIO_SPD_VHIGH, GPIO_PULLDOWN, DISP_SPI_AF);
    gpio_config(DISP_MISO_PORT, DISP_MISO_PIN, GPIO_AF, GPIO_PP, GPIO_SPD_VHIGH, GPIO_PULLDOWN, DISP_SPI_AF);
    gpio_config(DISP_MOSI_PORT, DISP_MOSI_PIN, GPIO_AF, GPIO_PP, GPIO_SPD_VHIGH, GPIO_PULLDOWN, DISP_SPI_AF);
    /* control pins: CS, DC, RST as outputs */
    gpio_config(DISP_CS_PORT,  DISP_CS_PIN,  GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
    gpio_config(DISP_DC_PORT,  DISP_DC_PIN,  GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
    gpio_config(DISP_RST_PORT, DISP_RST_PIN, GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
    gpio_set(DISP_CS_PORT, DISP_CS_PIN);
    gpio_set(DISP_DC_PORT, DISP_DC_PIN);
    gpio_set(DISP_RST_PORT, DISP_RST_PIN);

    /* ---- SPI5 (flash): SCK PF7, MISO PF8, MOSI PF9, AF5 ---- */
    gpio_config(FLASH_SCK_PORT,  FLASH_SCK_PIN,  GPIO_AF, GPIO_PP, GPIO_SPD_VHIGH, GPIO_NOPULL, FLASH_SPI_AF);
    gpio_config(FLASH_MISO_PORT, FLASH_MISO_PIN, GPIO_AF, GPIO_PP, GPIO_SPD_VHIGH, GPIO_NOPULL, FLASH_SPI_AF);
    gpio_config(FLASH_MOSI_PORT, FLASH_MOSI_PIN, GPIO_AF, GPIO_PP, GPIO_SPD_VHIGH, GPIO_NOPULL, FLASH_SPI_AF);
    gpio_config(FLASH_CS_PORT,   FLASH_CS_PIN,   GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
    gpio_set(FLASH_CS_PORT, FLASH_CS_PIN);

    /* Peripheral clocks (SPI5 + SPI6 are on APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_SPI5EN | RCC_APB2ENR_SPI6EN;
    (void)RCC->APB2ENR;

    spi_setup(DISP_SPI,  SPI_DIV4);  /* ~21 MHz display (Prusa's safe rate, 2x bring-up) */
    spi_setup(FLASH_SPI, SPI_DIV8);  /* ~10.5 MHz flash */
}

uint8_t spi_xfer(SPI_TypeDef *spi, uint8_t b) {
    while (!(spi->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&spi->DR = b;
    while (!(spi->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&spi->DR;
}

void spi_write(SPI_TypeDef *spi, const uint8_t *buf, size_t n) {
    /* TX-only, pipelined: feed the next byte as soon as the TX buffer is empty,
     * without waiting for each byte's RX round-trip (the display sends nothing
     * back). ~2x throughput vs. round-trip polling. Clock stays at the proven
     * 21 MHz, so this only removes the redundant wait. */
    for (size_t i = 0; i < n; i++) {
        while (!(spi->SR & SPI_SR_TXE)) { }
        *(volatile uint8_t *)&spi->DR = buf[i];
    }
    while (spi->SR & SPI_SR_BSY) { }   /* let the last byte finish shifting out */
    (void)spi->DR;                      /* drain RX, clear any overrun */
    (void)spi->SR;
}

void spi_read(SPI_TypeDef *spi, uint8_t *buf, size_t n) {
    for (size_t i = 0; i < n; i++) {
        buf[i] = spi_xfer(spi, 0xFF);
    }
}
