/* ILI9488 480x320 RGB666 display on SPI6 (xBuddy). */
#pragma once
#include <stddef.h>
#include <stdint.h>

void ili_init(void);
void ili_brightness(uint8_t b);
void ili_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h); /* leaves bus ready for pixel data */
void ili_push(const uint8_t *rgb, size_t bytes);                      /* raw RGB666 triplets after set_window */
void ili_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b);
void ili_clear(uint8_t r, uint8_t g, uint8_t b);
