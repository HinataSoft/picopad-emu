/* Headless run of a game .exe: virtual time, no window, no audio device,
   input script, PNG shots, optional WAV, machine-readable log on stdout.
   C without SDK headers (uses SDL threads/time only, never SDL_Init). */
#ifndef PICOPAD_PLATFORM_HEADLESS_H
#define PICOPAD_PLATFORM_HEADLESS_H

#include "cli.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Runs the game headless; returns the process exit code (0 quit/exit,
   1 fatal, 2 script error, 3 --max-time). May end the process itself
   (TerminateProcess) when the game thread hangs after an end request. */
int headless_run(const cli_options* opt, const char* game_name);

/* plat_* implementations used by platform/main.cpp while headless_run runs */
int headless_tick(unsigned long long t_us);
void headless_audio_write(const short* s, int n);
unsigned int headless_key_bits(void);
void headless_present(const unsigned short* fb, int x, int y, int w, int h);

#ifdef __cplusplus
}
#endif
#endif
