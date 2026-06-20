/* GBADoom platform backend for STM32F427 / xBuddy.
 * Implements the engine's *_e32 interface against our ILI9488 + encoder drivers.
 *
 * Engine renders 240x160 8-bit paletted. We scale 2x -> 480x320 RGB666 and
 * stream to the ILI9488. Framebuffer lives in CCM RAM to save main SRAM. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "doomdef.h"
#include "doomtype.h"
#include "d_event.h"
#include "d_main.h"
#include "i_system_e32.h"

#include "../board.h"
#include "../gpio.h"
#include "../spi.h"
#include "../ili9488.h"
#include "../input.h"

extern uint32_t millis(void);
extern void delay_us(uint32_t us);
extern void delay_ms(uint32_t ms);

/* Diagnostic: n short beeps on the buzzer (PA0) so we can hear how far init got
 * even with a black screen. */
void dbg_beep(int n) {
#ifndef DBG_BEEPS
    (void)n; return;           /* milestone beeps disabled now that boot works */
#endif
    gpio_config(GPIOA, 0, GPIO_OUT, GPIO_PP, GPIO_SPD_HIGH, GPIO_NOPULL, 0);
    for (int k = 0; k < n; k++) {
        for (int i = 0; i < 270; i++) { /* ~100 ms tone @ ~2.7 kHz */
            gpio_set(GPIOA, 0);   delay_us(185);
            gpio_clear(GPIOA, 0); delay_us(185);
        }
        delay_ms(140);
    }
    delay_ms(400);
}

#define FB_W 240
#define FB_H 160

/* On-screen output rectangle (centered). Smaller than 480x320 = fewer bytes over
 * SPI = higher framerate. The panel tops out at clean 21 MHz, so framerate scales
 * with the pixel count. Tune OUT_W/OUT_H to trade size vs. smoothness:
 *   240x160 (1x)  -> ~20 fps, quarter screen      480x320 (2x) -> ~5 fps, full
 *   360x240 (1.5x)-> ~10 fps, ~56% of the screen   <- current */
/* On the CORE One the ILI9488 needs MADCTL 0x40 + a transposed blit to render
 * upright (unlike the MK4's 0xE0). Doom's 240x160 frame is shown by pushing each
 * Doom column as a vertical screen strip. VIEW_W = on-screen width (Doom width
 * direction), VIEW_H = on-screen height (Doom height direction). 320x240 = 4:3. */
#define VIEW_W 320   /* on-screen width  (full panel width) */
#define VIEW_H 240   /* on-screen height (4:3 — Doom's intended aspect) */
/* Centered: window offsets along the CASET (320) and RASET (480) axes. */
#define VIEW_X ((DISP_W - VIEW_H) / 2)
#define VIEW_Y ((DISP_H - VIEW_W) / 2)

/* 240x160 8-bit paletted framebuffer in CCM (CPU-only, not DMA). */
static byte framebuffer[FB_W * FB_H] __attribute__((section(".ccmram_bss")));

/* Precomputed palette -> RGB666 (3 bytes/entry) for fast blit. */
static uint8_t pal666[256][3];

/* One on-screen vertical strip (a Doom column), VIEW_H pixels * 3 bytes. */
static uint8_t out_line[VIEW_H * 3];

void I_InitScreen_e32(void) {
    /* Low-level HW (clock done in SystemInit) brought up in main(); display init here. */
    ili_init();
    ili_clear(0, 0, 0);
}

void I_CreateBackBuffer_e32(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
}

unsigned short *I_GetBackBuffer(void) {
    return (unsigned short *)framebuffer;
}

unsigned short *I_GetFrontBuffer(void) {
    return (unsigned short *)framebuffer;
}

int I_GetVideoWidth_e32(void) { return 120; }   /* in shorts (240 px) */
int I_GetVideoHeight_e32(void) { return 160; }

void I_SetPallete_e32(const byte *pal) {
    for (int i = 0; i < 256; i++) {
        /* Doom palette is R,G,B 0..255; CORE One panel wants B,G,R byte order,
         * keeping the top 6 bits (RGB666). */
        pal666[i][0] = pal[i * 3 + 2] & 0xFC; /* B */
        pal666[i][1] = pal[i * 3 + 1] & 0xFC; /* G */
        pal666[i][2] = pal[i * 3 + 0] & 0xFC; /* R */
    }
}

void I_FinishUpdate_e32(const byte *srcBuffer, const byte *pallete,
                        const unsigned int width, const unsigned int height) {
    (void)pallete; (void)width; (void)height;
    const byte *src = srcBuffer ? srcBuffer : framebuffer;

    static int first = 1;
    if (first) {
        first = 0;
        ili_clear(0, 0, 0);   /* black the whole screen once so the border isn't garbage */
    }

    /* Window is transposed: CASET spans VIEW_H (screen vertical), RASET spans
     * VIEW_W (screen horizontal); offset to center on the panel. */
    ili_set_window(VIEW_X, VIEW_Y, VIEW_H, VIEW_W);

    for (unsigned vx = 0; vx < VIEW_W; vx++) {           /* screen column = Doom column */
        unsigned sx = (VIEW_W - 1 - vx) * FB_W / VIEW_W;  /* reversed -> un-mirrored horizontally */
        uint8_t *o = out_line;
        for (unsigned c = 0; c < VIEW_H; c++) {          /* strip top->bottom */
            unsigned sy = (VIEW_H - 1 - c) * FB_H / VIEW_H;  /* reversed (MX) so sky is up */
            const uint8_t *col = pal666[src[sx + sy * FB_W]];
            o[0] = col[0]; o[1] = col[1]; o[2] = col[2];
            o += 3;
        }
        ili_push(out_line, VIEW_H * 3);
    }
}

/* ---- Boot "PRESS START" screen ----------------------------------------
 * A tiny 5x7 font (only the glyphs we need) drawn into the same framebuffer and
 * pushed through the normal blit, so it lands upright and centered like the game. */
static const uint8_t *font_glyph(char ch) {
    static const uint8_t SP[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    static const uint8_t D[8] = { 0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0 };
    static const uint8_t O[8] = { 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0 };
    static const uint8_t M[8] = { 0x88, 0xD8, 0xA8, 0xA8, 0x88, 0x88, 0x88, 0 };
    static const uint8_t P[8] = { 0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80, 0 };
    static const uint8_t R[8] = { 0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88, 0 };
    static const uint8_t E[8] = { 0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0 };
    static const uint8_t S[8] = { 0x78, 0x80, 0x80, 0x70, 0x08, 0x08, 0xF0, 0 };
    static const uint8_t T[8] = { 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0 };
    static const uint8_t A[8] = { 0x70, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0 };
    switch (ch) {
        case 'D': return D; case 'O': return O; case 'M': return M;
        case 'P': return P; case 'R': return R; case 'E': return E;
        case 'S': return S; case 'T': return T; case 'A': return A;
        default:  return SP;
    }
}

static void fb_block(int x, int y, int w, int h, uint8_t color) {
    for (int j = 0; j < h; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= FB_H) continue;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            if (xx < 0 || xx >= FB_W) continue;
            framebuffer[yy * FB_W + xx] = color;
        }
    }
}

static void draw_str_centered(const char *str, int y, int s, uint8_t color) {
    int n = 0;
    for (const char *p = str; *p; p++) n++;
    int x = (FB_W - (n * 6 * s - s)) / 2;     /* 5px glyph + 1px gap, no trailing gap */
    for (const char *p = str; *p; p++) {
        const uint8_t *g = font_glyph(*p);
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (g[r] & (0x80 >> c))
                    fb_block(x + c * s, y + r * s, s, s, color);
        x += 6 * s;
    }
}

/* Classic "Doom fire": heat seeded at the bottom row propagates upward with a
 * little random decay/drift. Indices 0..36 are the fire ramp; index 37 = white
 * (text) and is left untouched by the spread. */
#define FIRE_WHITE 37
static uint32_t fire_seed = 1234567u;
static void fire_step(void) {
    for (int x = 0; x < FB_W; x++) {
        framebuffer[(FB_H - 1) * FB_W + x] = 36;   /* source row */
    }
    for (int x = 0; x < FB_W; x++) {
        for (int y = 1; y < FB_H; y++) {
            int from = y * FB_W + x;
            uint8_t p = framebuffer[from];
            if (p == 0) { framebuffer[from - FB_W] = 0; continue; }
            if (p >= FIRE_WHITE) continue;          /* don't spread the text */
            fire_seed = fire_seed * 1664525u + 1013904223u;
            int r = (fire_seed >> 16) & 3;
            int up = from - r + 1 - FB_W;
            if (up >= 0 && up < FB_W * FB_H) {
                framebuffer[up] = p - (r & 1);
            }
        }
    }
}

void I_StartScreen_e32(void) {
    static const uint8_t fire_rgb[37 * 3] = {
        0x07,0x07,0x07, 0x1F,0x07,0x07, 0x2F,0x0F,0x07, 0x47,0x0F,0x07,
        0x57,0x17,0x07, 0x67,0x1F,0x07, 0x77,0x1F,0x07, 0x8F,0x27,0x07,
        0x9F,0x2F,0x07, 0xAF,0x3F,0x07, 0xBF,0x47,0x07, 0xC7,0x47,0x07,
        0xDF,0x4F,0x07, 0xDF,0x57,0x07, 0xDF,0x57,0x07, 0xD7,0x5F,0x07,
        0xD7,0x5F,0x07, 0xD7,0x67,0x0F, 0xCF,0x6F,0x0F, 0xCF,0x77,0x0F,
        0xCF,0x7F,0x0F, 0xCF,0x87,0x17, 0xC7,0x87,0x17, 0xC7,0x8F,0x17,
        0xC7,0x97,0x1F, 0xBF,0x9F,0x1F, 0xBF,0x9F,0x1F, 0xBF,0xA7,0x27,
        0xBF,0xA7,0x27, 0xBF,0xAF,0x2F, 0xB7,0xAF,0x2F, 0xB7,0xB7,0x2F,
        0xB7,0xB7,0x37, 0xCF,0xCF,0x6F, 0xDF,0xDF,0x9F, 0xEF,0xEF,0xC7,
        0xFF,0xFF,0xFF,
    };
    for (int i = 0; i < 37; i++) {
        pal666[i][0] = fire_rgb[i * 3 + 2] & 0xFC;  /* B */
        pal666[i][1] = fire_rgb[i * 3 + 1] & 0xFC;  /* G */
        pal666[i][2] = fire_rgb[i * 3 + 0] & 0xFC;  /* R */
    }
    pal666[FIRE_WHITE][0] = 0xFC; pal666[FIRE_WHITE][1] = 0xFC; pal666[FIRE_WHITE][2] = 0xFC;
    memset(framebuffer, 0, sizeof(framebuffer));

    for (;;) {
        fire_step();
        draw_str_centered("DOOM", 22, 4, FIRE_WHITE);
        draw_str_centered("PRESS START", 64, 2, FIRE_WHITE);
        I_FinishUpdate_e32(0, 0, 0, 0);
        if (input_button_down()) {
            /* keep animating until released so the start-click doesn't leak into play */
            while (input_button_down()) {
                fire_step();
                draw_str_centered("DOOM", 22, 4, FIRE_WHITE);
                draw_str_centered("PRESS START", 64, 2, FIRE_WHITE);
                I_FinishUpdate_e32(0, 0, 0, 0);
            }
            return;
        }
    }
}

/* 35 Hz Doom tics from the 1 kHz SysTick. */
int I_GetTime_e32(void) {
    return (int)((millis() * TICRATE) / 1000u);
}

/* ---- Input: rotary encoder + button -> Doom key events ----
 * GBADoom bindings: forward=KEYD_UP, turn=KEYD_LEFT/RIGHT, fire=KEYD_B, use=KEYD_A.
 * With one encoder + one button:
 *   encoder turn      -> turn left/right (key held while actively turning)
 *   button held       -> walk forward
 *   button short tap  -> fire + use (open doors / shoot)
 * Keys must stay "down" for at least one tic, so we hold them with timers rather
 * than pulsing keydown+keyup in a single pass (which the engine would cancel out).
 * The encoder itself is polled at 1 kHz from SysTick (see g_systick_hook). */
static void post(int type, int key) {
    event_t ev;
    ev.type = type;
    ev.data1 = key;
    D_PostEvent(&ev);
}

void I_ProcessKeyEvents(void) {
    uint32_t now = millis();

    /* encoder -> turn, holding the key while the knob keeps moving */
    static int turn = 0; /* -1 left, +1 right */
    static uint32_t turn_t = 0;
    int d = -input_take_delta();   /* inverted to match the (un-mirrored) view */
    if (d > 0) {
        if (turn <= 0) { if (turn < 0) post(ev_keyup, KEYD_LEFT); post(ev_keydown, KEYD_RIGHT); turn = 1; }
        turn_t = now;
    } else if (d < 0) {
        if (turn >= 0) { if (turn > 0) post(ev_keyup, KEYD_RIGHT); post(ev_keydown, KEYD_LEFT); turn = -1; }
        turn_t = now;
    }
    if (turn && (now - turn_t) > 120) {
        post(ev_keyup, turn > 0 ? KEYD_RIGHT : KEYD_LEFT);
        turn = 0;
    }

    /* button -> hold=forward, tap=fire+use */
    int btn = input_button_down();
    static int was = 0, fwd = 0, firing = 0;
    static uint32_t down_t = 0, fire_t = 0;
    if (btn && !was) {
        was = 1; fwd = 0; down_t = now;
    } else if (btn && was && !fwd && (now - down_t) > 250) {
        post(ev_keydown, KEYD_UP); fwd = 1;
    } else if (!btn && was) {
        was = 0;
        if (fwd) { post(ev_keyup, KEYD_UP); fwd = 0; }
        else { post(ev_keydown, KEYD_B); post(ev_keydown, KEYD_A); firing = 1; fire_t = now; }
    }
    if (firing && (now - fire_t) > 80) {
        post(ev_keyup, KEYD_B); post(ev_keyup, KEYD_A); firing = 0;
    }
}

void I_Quit_e32(void) {
    for (;;) { }
}

#define MAX_MESSAGE_SIZE 256
void I_Error(const char *error, ...) {
    char msg[MAX_MESSAGE_SIZE];
    va_list v;
    va_start(v, error);
    vsnprintf(msg, sizeof(msg), error, v);
    va_end(v);
    /* Show a red screen so a fatal error is visible on the panel. */
    ili_clear(0xFC, 0x00, 0x00);
    for (;;) { }
}
