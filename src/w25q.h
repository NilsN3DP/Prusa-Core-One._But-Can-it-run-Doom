/* W25Q (8 MB) SPI NOR flash reader on SPI5 — holds the DOOM WAD. */
#pragma once
#include <stddef.h>
#include <stdint.h>

void w25q_read_jedec_id(uint8_t id[3]);     /* [manufacturer, mem-type, capacity] */
void w25q_read(uint32_t addr, uint8_t *buf, size_t n);
