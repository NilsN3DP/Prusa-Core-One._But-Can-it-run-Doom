/* Play tones on the X/Y stepper motors (the printer's own "music"). */
#pragma once

void tmc_music_init(void);   /* init TMC2130 on X+Y over SPI3, energize the motors */
void tmc_music_start(void);  /* start the song */
void tmc_music_tick(void);   /* must be called at 40 kHz (from SysTick) */
