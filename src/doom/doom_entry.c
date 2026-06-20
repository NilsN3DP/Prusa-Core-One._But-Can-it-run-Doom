/* Entry point for the DOOM build (replaces GBADoom's i_main.c).
 * Brings up xBuddy hardware, then runs the Doom engine. */

#include "doomdef.h"
#include "d_main.h"
#include "i_video.h"
#include "z_zone.h"
#include "global_data.h"

#include "../gpio.h"
#include "../spi.h"
#include "../input.h"
#include "tmc_music.h"
#include "fx.h"

/* GBADoom's I_Init (in i_main.c) starts sound; we run without audio for now. */
void I_Init(void) {
}

extern void dbg_beep(int n);
extern void (*g_systick_hook)(void);
extern void I_StartScreen_e32(void);
extern void delay_ms(uint32_t ms);

/* SysTick @ 40 kHz: drive the stepper-music DDS every tick, poll the encoder at
 * ~1 kHz (every 40th tick). */
static void doom_systick(void) {
    tmc_music_tick();
    fx_tick();
    static uint32_t c = 0;
    if (++c >= 40) { c = 0; input_poll(); }
}

int main(void) {
    /* Clock + SysTick already configured in SystemInit (Reset_Handler). */
    gpio_enable_all_clocks();
    spi_init_pins_and_periph();
    input_init();

    /* Silence the two main-board fans (print PE11, heatbreak PE9) so the stepper
     * music is audible — their pins otherwise float. Safe: this firmware never
     * heats the hotend. The chamber/filtration fans sit on the extension board
     * and can't be reached from here. */
    gpio_config(GPIOE, 11, GPIO_OUT, GPIO_PP, GPIO_SPD_LOW, GPIO_NOPULL, 0);
    gpio_config(GPIOE, 9,  GPIO_OUT, GPIO_PP, GPIO_SPD_LOW, GPIO_NOPULL, 0);
    gpio_clear(GPIOE, 11);
    gpio_clear(GPIOE, 9);

    fx_init();             /* buzzer (PA0) + door-fan (PE11) for sound effects */

    /* Configure the stepper drivers now, but keep the motors disabled (silent)
     * and the music stopped until the player presses start. The SysTick hook
     * already polls the encoder so the button works on the boot screen. */
    tmc_music_init();
    g_systick_hook = doom_systick;

    I_PreInitGraphics();   /* -> I_InitScreen_e32 (ili_init) */

    /* Boot gate: animated fire screen; returns after a full click of the knob. */
    I_StartScreen_e32();

    tmc_music_start();     /* energize motors + start "At Doom's Gate" */

    I_Init();
    Z_Init();              /* allocates the zone heap via malloc */
    InitGlobals();

    D_DoomMain();          /* runs the game loop */

    for (;;) { }
    return 0;
}
