/* Buzzer sound-effects + door-triggered fan, driven by Doom sound events. */
#pragma once

void fx_init(void);        /* configure buzzer (PA0) + part-fan (PE11) */
void fx_sound(int sfxid);  /* called from I_StartSound: map sfx -> buzzer (+ fan on doors) */
void fx_tick(void);        /* call at 40 kHz from SysTick */
