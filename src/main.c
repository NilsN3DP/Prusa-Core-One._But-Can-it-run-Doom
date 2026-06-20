/* Standalone DOOM firmware for Prusa xBuddy — Phase A hardware bring-up.
 * Verifies: 168 MHz clock, ILI9488 display, W25Q flash SPI, rotary encoder + button.
 * Visual result on the printer screen:
 *   - top: six colour bars (display + addressing OK)
 *   - small square below bars: GREEN if W25Q flash answers with Winbond id, else RED
 *   - movable block: turn knob to move it, press knob to turn it red */

#include "stm32f4xx.h"
#include "board.h"
#include "gpio.h"
#include "spi.h"
#include "ili9488.h"
#include "w25q.h"
#include "input.h"

extern void delay_ms(uint32_t ms);
extern void delay_us(uint32_t us);

/* Buzzer on PA0 (CORE One). Square-wave a passive piezo via plain GPIO toggling.
 * Used as an "I'm alive" signal so we can tell our firmware booted even if the
 * display stays dark. */
static void beep(uint32_t freq_hz, uint32_t ms) {
    gpio_config(GPIOA, 0, GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
    uint32_t half_us = 500000u / freq_hz;
    uint32_t cycles = (ms * 1000u) / (half_us * 2u);
    for (uint32_t i = 0; i < cycles; i++) {
        gpio_set(GPIOA, 0);
        delay_us(half_us);
        gpio_clear(GPIOA, 0);
        delay_us(half_us);
    }
    gpio_clear(GPIOA, 0);
}

static void draw_color_bars(void) {
    const uint8_t bars[6][3] = {
        { 0xFC, 0x00, 0x00 }, /* red */
        { 0x00, 0xFC, 0x00 }, /* green */
        { 0x00, 0x00, 0xFC }, /* blue */
        { 0xFC, 0xFC, 0x00 }, /* yellow */
        { 0x00, 0xFC, 0xFC }, /* cyan */
        { 0xFC, 0xFC, 0xFC }, /* white */
    };
    const uint16_t bw = DISP_W / 6;
    for (int i = 0; i < 6; i++) {
        ili_fill_rect((uint16_t)(i * bw), 0, bw, 60, bars[i][0], bars[i][1], bars[i][2]);
    }
}

int main(void) {
    gpio_enable_all_clocks();

    /* "Alive" beep BEFORE touching the display: if you hear this, our firmware
     * booted (bootloader handed off OK) and any remaining issue is the display. */
    beep(2700, 150);

    spi_init_pins_and_periph();
    input_init();
    ili_init();

    ili_clear(0x10, 0x10, 0x10); /* dark grey background */
    draw_color_bars();

    /* Two short beeps AFTER display init returned (display didn't hang). */
    beep(3500, 80);
    delay_ms(60);
    beep(3500, 80);

    /* W25Q flash check */
    uint8_t id[3] = { 0, 0, 0 };
    w25q_read_jedec_id(id);
    int flash_ok = (id[0] == 0xEF); /* Winbond */
    if (flash_ok) {
        ili_fill_rect(10, 80, 40, 40, 0x00, 0xFC, 0x00);
    } else {
        ili_fill_rect(10, 80, 40, 40, 0xFC, 0x00, 0x00);
    }

    /* Movable block */
    const int sq = 48;
    const int y = 160;
    int pos = (DISP_W - sq) / 2;
    int last_pos = -1;
    int last_btn = -1;
    int last_a = -1, last_b = -1;

    for (;;) {
        input_poll();                 /* poll every iteration (~2 kHz) */

        int d = input_take_raw();     /* raw quadrature transitions, max sensitivity */
        if (d) {
            pos += d * 4;
            if (pos < 0) pos = 0;
            if (pos > DISP_W - sq) pos = DISP_W - sq;
        }
        int btn = input_button_down();

        /* Live encoder-line indicators: A at (380,80), B at (430,80).
         * Turn the knob — if these flicker, the pins see the encoder. */
        int a = input_raw_a(), b = input_raw_b();
        if (a != last_a) {
            ili_fill_rect(380, 80, 30, 30, a ? 0xFC : 0x30, a ? 0xFC : 0x30, a ? 0xFC : 0x30);
            last_a = a;
        }
        if (b != last_b) {
            ili_fill_rect(430, 80, 30, 30, b ? 0xFC : 0x30, b ? 0xFC : 0x30, b ? 0xFC : 0x30);
            last_b = b;
        }

        if (pos != last_pos || btn != last_btn) {
            if (last_pos >= 0 && last_pos != pos) {
                ili_fill_rect((uint16_t)last_pos, y, sq, sq, 0x10, 0x10, 0x10); /* erase */
            }
            ili_fill_rect((uint16_t)pos, y, sq, sq, 0xFC, btn ? 0x00 : 0xFC, btn ? 0x00 : 0xFC);
            last_pos = pos;
            last_btn = btn;
        }
        delay_us(500);
    }
    return 0;
}
