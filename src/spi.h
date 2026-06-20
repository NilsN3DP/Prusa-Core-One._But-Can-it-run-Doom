/* SPI5 (flash) + SPI6 (display) master drivers for xBuddy. */
#pragma once
#include "stm32f4xx.h"
#include <stddef.h>
#include <stdint.h>

/* Baud prescaler codes for CR1.BR[5:3] */
enum { SPI_DIV2 = 0, SPI_DIV4 = 1, SPI_DIV8 = 2, SPI_DIV16 = 3,
       SPI_DIV32 = 4, SPI_DIV64 = 5, SPI_DIV128 = 6, SPI_DIV256 = 7 };

void spi_init_pins_and_periph(void); /* configures GPIO + SPI5 + SPI6 + control pins */

uint8_t spi_xfer(SPI_TypeDef *spi, uint8_t b);
void    spi_write(SPI_TypeDef *spi, const uint8_t *buf, size_t n);
void    spi_read(SPI_TypeDef *spi, uint8_t *buf, size_t n);
void    spi_set_baud(SPI_TypeDef *spi, int div_code);
