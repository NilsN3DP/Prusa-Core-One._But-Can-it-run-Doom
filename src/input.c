#include "input.h"
#include "board.h"
#include "gpio.h"

static const int8_t qtab[16] = {
    0, -1, +1,  0,
   +1,  0,  0, -1,
   -1,  0,  0, +1,
    0, +1, -1,  0
};

static uint8_t prev_ab;
static volatile int accum;

void input_init(void) {
    gpio_config(ENC_A_PORT,   ENC_A_PIN,   GPIO_IN, GPIO_PP, GPIO_SPD_LOW, GPIO_PULLUP, 0);
    gpio_config(ENC_B_PORT,   ENC_B_PIN,   GPIO_IN, GPIO_PP, GPIO_SPD_LOW, GPIO_PULLUP, 0);
    gpio_config(ENC_BTN_PORT, ENC_BTN_PIN, GPIO_IN, GPIO_PP, GPIO_SPD_LOW, GPIO_PULLUP, 0);
    prev_ab = (uint8_t)((gpio_read(ENC_A_PORT, ENC_A_PIN) << 1) | gpio_read(ENC_B_PORT, ENC_B_PIN));
    accum = 0;
}

void input_poll(void) {
    uint8_t ab = (uint8_t)((gpio_read(ENC_A_PORT, ENC_A_PIN) << 1) | gpio_read(ENC_B_PORT, ENC_B_PIN));
    uint8_t idx = (uint8_t)((prev_ab << 2) | ab);
    accum += qtab[idx & 0x0F];
    prev_ab = ab;
}

int input_take_delta(void) {
    int d = accum;
    accum = 0;
    /* 4 quadrature transitions per detent */
    return d / 4;
}

int input_take_raw(void) {
    int d = accum;
    accum = 0;
    return d;
}

int input_button_down(void) {
    return gpio_read(ENC_BTN_PORT, ENC_BTN_PIN) == 0; /* active low */
}

int input_raw_a(void) { return gpio_read(ENC_A_PORT, ENC_A_PIN); }
int input_raw_b(void) { return gpio_read(ENC_B_PORT, ENC_B_PIN); }
