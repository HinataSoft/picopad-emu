// ****************************************************************************
//
//                 Host runtime - internal interface (host/ only)
//
// ****************************************************************************
// Shared by the host implementation files (clock.c, pump.c, runtime.c and the
// display/keys/audio models). Not for games, platform/ or tests.
// Plain C types only (included from TUs that also see the SDK headers).

#ifndef _HOST_INTERNAL_H
#define _HOST_INTERNAL_H

#include "../include/plat_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- clock.c

// configuration set by host_configure() (never NULL)
const host_config* host_cfg(void);

// wait 'us' microseconds of emulated time, running the SysTick pump:
//  real clock - sleeps in steps of <= 1 ms and pumps between them;
//  virtual clock - advances the virtual counter by 'us' and pumps once
void host_wait_us(unsigned long long us);

// restart emulated time at 0 (called by host_run_game)
void host_clock_reset(void);

// --- pump.c

// 1 when called on the game thread (the thread inside host_run_game)
int host_is_game_thread(void);

// mark/unmark the calling thread as the game thread (host_run_game)
void host_set_game_thread(int on);

// reset SysTick phase, spin guard, reentrancy flag and IRQ lock state of the
// calling thread (start of host_run_game and after longjmp out of the game)
void host_pump_reset(void);

// --- audio.c

// game thread (from host_pump): produce the sound samples due by emulated
// time (runs the stored PWM-wrap handler once per sample)
void host_audio_pump(void);
void host_audio_reset(void);		// restart the produced-sample counter (host_run_game)

// --- runtime.c

// game entry: C++ shim host/game_entry.cpp in game builds, defined by C tests
int host_call_game_main(void);

// fatal error: plat_log(msg) and leave host_run_game with exit code 1
void host_fatal(const char* msg) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif // _HOST_INTERNAL_H
