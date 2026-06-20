/* GBADoom's i_audio.c was GBA/maxmod-specific. Music plays on the steppers;
 * here we forward each sound event to the buzzer (and the fan on doors). */

#include "i_sound.h"
#include "fx.h"

void I_InitSound(void) {}
int  I_StartSound(int id, int channel, int vol, int sep) {
    (void)channel; (void)vol; (void)sep;
    fx_sound(id);   /* buzzer blip + fan whoosh on doors */
    return -1;
}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
void I_PlaySong(int handle, int looping) { (void)handle; (void)looping; }
void I_StopSong(int handle) { (void)handle; }
