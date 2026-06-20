/* "At Doom's Gate" on the X/Y/Z/E stepper motors — 4-voice polyphony.
 *
 * All four TMC2130 drivers share SPI3 (SCK PC10, MISO PC11, MOSI PC12, AF6);
 * each has its own chip-select. The four STEP and four DIR lines all live on
 * GPIOD, so every voice is clocked with a single BSRR write per tick.
 *
 *   voice  motor  STEP  DIR   CS      enable        role
 *   0      X      PD7   PD6   PG15    PB9 (XY)      melody / main riff
 *   1      Y      PD5   PD4   PB5     PB9 (XY)      harmony
 *   2      Z      PD3   PD2   PF15    PB8           bass (lead screw)
 *   3      E      PD1   PD0   PF12    PD10          doubles the riff
 *
 * Tones come from a per-voice DDS phase accumulator ticked at 40 kHz from
 * SysTick; DIR flips every few steps so each axis only dithers in place. */

#include "stm32f4xx.h"
#include "../gpio.h"

/* TMC2130 chip-selects (all on SPI3) — order: X, Y, Z, E */
static GPIO_TypeDef *const cs_port[4] = { GPIOG, GPIOB, GPIOF, GPIOF };
static const uint8_t       cs_pin[4]  = { 15,    5,     15,    12   };

/* STEP / DIR — all on GPIOD */
static const uint8_t step_pin[4] = { 7, 5, 3, 1 };
static const uint8_t dir_pin[4]  = { 6, 4, 2, 0 };

/* ---- TMC2130 registers ---- */
#define REG_GCONF      0x00
#define REG_IHOLD_IRUN 0x10
#define REG_TPOWERDOWN 0x11
#define REG_CHOPCONF   0x6C

static uint8_t spi3_xfer(uint8_t b) {
    while (!(SPI3->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI3->DR = b;
    while (!(SPI3->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&SPI3->DR;
}

static void tmc_write(GPIO_TypeDef *port, int pin, uint8_t reg, uint32_t val) {
    gpio_clear(port, pin);
    spi3_xfer(reg | 0x80);                 /* write bit */
    spi3_xfer((val >> 24) & 0xFF);
    spi3_xfer((val >> 16) & 0xFF);
    spi3_xfer((val >> 8) & 0xFF);
    spi3_xfer(val & 0xFF);
    gpio_set(port, pin);
}

static void tmc_setup(GPIO_TypeDef *port, int pin) {
    tmc_write(port, pin, REG_GCONF, 0x00000000);      /* SpreadCycle (audible) */
    tmc_write(port, pin, REG_TPOWERDOWN, 0x0000000A);
    /* CHOPCONF: TOFF=3, TBL=2, vsense=0, MRES=4 -> 16 microsteps */
    tmc_write(port, pin, REG_CHOPCONF, 0x040100C3);
    /* IHOLD_IRUN: IHOLD=8, IRUN=16 (~0.55 A rms @ RSENSE 0.22), IHOLDDELAY=6 */
    tmc_write(port, pin, REG_IHOLD_IRUN, 0x00061008);
}

void tmc_music_init(void) {
    /* SPI3 pins PC10/11/12, AF6 */
    gpio_config(GPIOC, 10, GPIO_AF, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 6);
    gpio_config(GPIOC, 11, GPIO_AF, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 6);
    gpio_config(GPIOC, 12, GPIO_AF, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 6);

    for (int v = 0; v < 4; v++) {
        gpio_config(cs_port[v], cs_pin[v], GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
        gpio_set(cs_port[v], cs_pin[v]);
        gpio_config(GPIOD, step_pin[v], GPIO_OUT, GPIO_PP, GPIO_SPD_VHIGH, GPIO_NOPULL, 0);
        gpio_config(GPIOD, dir_pin[v],  GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH,  GPIO_NOPULL, 0);
    }

    /* SPI3 master, mode 3, 8-bit, ~1.3 MHz (APB1 84/64), soft NSS */
    RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
    (void)RCC->APB1ENR;
    SPI3->CR1 = 0;
    SPI3->CR1 = (5u << 3)                       /* BR /64 */
              | SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI
              | SPI_CR1_CPOL | SPI_CR1_CPHA;
    SPI3->CR2 = 0;
    SPI3->CR1 |= SPI_CR1_SPE;

    for (int v = 0; v < 4; v++) {
        tmc_setup(cs_port[v], cs_pin[v]);
    }

    /* Enable lines XY (PB9), Z (PB8), E (PD10) — active low. Leave the motors
     * DISABLED (high) until tmc_music_start() so they stay silent on the boot
     * screen; energizing only when the song begins. */
    gpio_config(GPIOB, 9,  GPIO_OUT, GPIO_PP, GPIO_SPD_LOW, GPIO_NOPULL, 0);
    gpio_config(GPIOB, 8,  GPIO_OUT, GPIO_PP, GPIO_SPD_LOW, GPIO_NOPULL, 0);
    gpio_config(GPIOD, 10, GPIO_OUT, GPIO_PP, GPIO_SPD_LOW, GPIO_NOPULL, 0);
    gpio_set(GPIOB, 9);
    gpio_set(GPIOB, 8);
    gpio_set(GPIOD, 10);
}

/* "At Doom's Gate" 4-voice arrangement, generated from D_E1M1 by
 * tools/extract_music.py. Provides song_x/y/z/e[] (Hz), song_d[] (ms), SONG_LEN. */
#include "doom_music.h"
static const uint16_t *const voice[4] = { song_x, song_y, song_z, song_e };

#define SR        40000u   /* SysTick rate */
#define REV_STEPS 64       /* flip direction this often -> tiny, safe sweep */

static volatile int s_playing = 0;
static uint32_t s_note_tick, s_note_idx;
static uint32_t v_phase[4];
static int v_scnt[4], v_dir[4];

void tmc_music_start(void) {
    /* energize the motors (active low) now that the song is starting */
    gpio_clear(GPIOB, 9);
    gpio_clear(GPIOB, 8);
    gpio_clear(GPIOD, 10);
    s_note_tick = s_note_idx = 0;
    for (int v = 0; v < 4; v++) { v_phase[v] = 0; v_scnt[v] = 0; v_dir[v] = 0; }
    s_playing = 1;
}

void tmc_music_tick(void) {
    if (!s_playing) {
        return;
    }
    uint32_t set_mask = 0;
    for (int v = 0; v < 4; v++) {
        uint16_t f = voice[v][s_note_idx];
        if (!f) {
            continue;
        }
        v_phase[v] += f;
        if (v_phase[v] >= SR) {
            v_phase[v] -= SR;
            set_mask |= (1u << step_pin[v]);
            if (++v_scnt[v] >= REV_STEPS) {
                v_scnt[v] = 0;
                v_dir[v] ^= 1;
                GPIOD->BSRR = v_dir[v] ? (1u << dir_pin[v])
                                       : (1u << (dir_pin[v] + 16));
            }
        }
    }
    if (set_mask) {
        GPIOD->BSRR = set_mask;                 /* all stepping voices high */
        for (volatile int i = 0; i < 8; i++) { } /* ~min STEP high time */
        GPIOD->BSRR = set_mask << 16;           /* and low again */
    }
    if (++s_note_tick >= (uint32_t)song_d[s_note_idx] * (SR / 1000u)) {
        s_note_tick = 0;
        if (++s_note_idx >= SONG_LEN) {
            s_note_idx = 0;
        }
    }
}
