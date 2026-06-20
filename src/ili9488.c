#include "ili9488.h"
#include "spi.h"
#include "board.h"
#include "gpio.h"

extern void delay_ms(uint32_t ms);

#define CMD_SLPOUT  0x11
#define CMD_INVON   0x21
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A
#define CMD_WRDISBV 0x51
#define CMD_WRCTRLD 0x53
#define CMD_CABC2   0xC8

static inline void dc_cmd(void)  { gpio_clear(DISP_DC_PORT, DISP_DC_PIN); }
static inline void dc_data(void) { gpio_set(DISP_DC_PORT, DISP_DC_PIN); }

static void wr_cmd(uint8_t c) {
    dc_cmd();
    spi_xfer(DISP_SPI, c);
    dc_data();
}

static void wr_cmd_data(uint8_t c, const uint8_t *d, size_t n) {
    dc_cmd();
    spi_xfer(DISP_SPI, c);
    dc_data();
    if (d && n) {
        spi_write(DISP_SPI, d, n);
    }
}

static void set_addr(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1) {
    uint8_t c[4];
    c[0] = x0 >> 8; c[1] = x0 & 0xFF; c[2] = x1 >> 8; c[3] = x1 & 0xFF;
    wr_cmd_data(CMD_CASET, c, 4);
    c[0] = y0 >> 8; c[1] = y0 & 0xFF; c[2] = y1 >> 8; c[3] = y1 & 0xFF;
    wr_cmd_data(CMD_RASET, c, 4);
}

void ili_set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    set_addr(x, x + w - 1, y, y + h - 1);
    wr_cmd(CMD_RAMWR); /* leaves DC=data, ready for pixels */
}

void ili_push(const uint8_t *rgb, size_t bytes) {
    spi_write(DISP_SPI, rgb, bytes);
}

void ili_brightness(uint8_t b) {
    wr_cmd_data(CMD_WRDISBV, &b, 1);
}

void ili_init(void) {
    /* CS held low for the whole session (xBuddy wiring). */
    gpio_clear(DISP_CS_PORT, DISP_CS_PIN);

    /* Hardware reset pulse */
    gpio_set(DISP_RST_PORT, DISP_RST_PIN);   delay_ms(5);
    gpio_clear(DISP_RST_PORT, DISP_RST_PIN); delay_ms(20);
    gpio_set(DISP_RST_PORT, DISP_RST_PIN);   delay_ms(120);

    /* --- ILI9488 init (matches Prusa "new manufacturer" path) --- */
    static const uint8_t adj_ctrl3[] = { 0xA9, 0x51, 0x2C, 0x82 };
    wr_cmd_data(0xF7, adj_ctrl3, 4);

    uint8_t v;
    v = 0x40; wr_cmd_data(CMD_MADCTL, &v, 1);   /* portrait 320x480 (CORE One); 0x80 = upside down */
    v = 0x66; wr_cmd_data(CMD_COLMOD, &v, 1);   /* RGB666, 3 bytes/pixel */

    static const uint8_t frc[] = { 0xA0, 0x11 };
    wr_cmd_data(0xB1, frc, 2);                   /* Frame Rate Control */
    v = 0x02; wr_cmd_data(0xB4, &v, 1);          /* Display Inversion Control (2-dot) */

    static const uint8_t pwr1[] = { 0x0F, 0x0F };
    wr_cmd_data(0xC0, pwr1, 2);                  /* Power Control 1 */
    v = 0x41; wr_cmd_data(0xC1, &v, 1);          /* Power Control 2 */
    v = 0x22; wr_cmd_data(0xC2, &v, 1);          /* Power Control 3 */

    static const uint8_t vcom[] = { 0x00, 0x53, 0x80 };
    wr_cmd_data(0xC5, vcom, 3);                  /* VCOM Control */
    v = 0xC6; wr_cmd_data(0xB7, &v, 1);          /* Entry Mode Set */

    static const uint8_t pgamma[] = { 0x00, 0x08, 0x0C, 0x02, 0x0E, 0x04, 0x30, 0x45,
                                      0x47, 0x04, 0x0C, 0x0A, 0x2E, 0x34, 0x0F };
    wr_cmd_data(0xE0, pgamma, sizeof(pgamma));
    static const uint8_t ngamma[] = { 0x00, 0x11, 0x0D, 0x01, 0x0F, 0x05, 0x39, 0x36,
                                      0x51, 0x06, 0x0F, 0x0D, 0x33, 0x37, 0x0F };
    wr_cmd_data(0xE1, ngamma, sizeof(ngamma));

    wr_cmd(CMD_INVON);                           /* display inversion ON */
    wr_cmd(CMD_SLPOUT);  delay_ms(120);          /* sleep out */
    wr_cmd(CMD_DISPON);  delay_ms(20);           /* display on */

    /* Backlight/brightness (display-internal, matches Prusa). */
    v = 0x24; wr_cmd_data(CMD_WRCTRLD, &v, 1);   /* BCTRL | BL */
    v = 0xB1; wr_cmd_data(CMD_CABC2, &v, 1);     /* CABCCTRL2: inverted PWM */
    ili_brightness(0xFF);                        /* max */
}

/* One landscape line of pixels as RGB666 triplets. */
static uint8_t line_buf[DISP_W * 3];

void ili_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b) {
    if (w == 0 || h == 0) return;
    if (w > DISP_W) w = DISP_W;
    /* CORE One panel reads pixel bytes as B,G,R (byte0 = blue). */
    for (uint16_t i = 0; i < w; i++) {
        line_buf[i * 3 + 0] = b;
        line_buf[i * 3 + 1] = g;
        line_buf[i * 3 + 2] = r;
    }
    ili_set_window(x, y, w, h);
    for (uint16_t row = 0; row < h; row++) {
        ili_push(line_buf, (size_t)w * 3);
    }
}

void ili_clear(uint8_t r, uint8_t g, uint8_t b) {
    ili_fill_rect(0, 0, DISP_W, DISP_H, r, g, b);
}
