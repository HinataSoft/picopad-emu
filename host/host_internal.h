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

// virtual clock only: raise the virtual counter to 'tick_us' (the time of the
// next SysTick, LastTick + 5000) if it is behind; never moves it back; atomic,
// called under the pump lock (game thread idle guard or watchdog)
void host_clock_advance_to_next_tick(unsigned long long tick_us);

// --- pump.c

// 1 when called on the game thread (the thread inside host_run_game)
int host_is_game_thread(void);

// mark/unmark the calling thread as the game thread (host_run_game)
void host_set_game_thread(int on);

// reset SysTick phase, spin guard, reentrancy flag and IRQ lock state of the
// calling thread (start of host_run_game and after longjmp out of the game)
void host_pump_reset(void);

// ask the game to end with exit code 'code' (>= 0); callable from any thread
// (watchdog); the next host_pump on the game thread calls host_exit(code)
void host_request_exit(int code);

// forget a pending exit request (start of host_run_game)
void host_pump_clear_exit(void);

// game thread: restart the virtual-clock idle count (host_wait_us)
void host_pump_idle_reset(void);

// game thread: 1 = a game runs (enables host_watchdog, resets the stall count;
// after the resets at host_run_game start), 0 = game finished (waits for a
// running watchdog step; before host_pump_reset at the end)
void host_pump_activate(int on);

// --- audio.c

// under the pump lock (host_pump on the game thread, or a host_watchdog
// step): produce the sound samples due by emulated time (runs the stored
// PWM-wrap handler once per sample)
void host_audio_pump(void);
void host_audio_reset(void);		// restart the produced-sample counter (host_run_game)

// --- runtime.c

// leave host_run_game with the given exit code (game thread only)
void host_exit(int code) __attribute__((noreturn));

// game entry: C++ shim host/game_entry.cpp in game builds, defined by C tests
int host_call_game_main(void);

// fatal error: plat_log(msg) and leave host_run_game with exit code 1
void host_fatal(const char* msg) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif // _HOST_INTERNAL_H
