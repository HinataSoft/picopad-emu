// ****************************************************************************
//
//                     Host runtime - game start and exit
//
// ****************************************************************************
// host_run_game() runs the game on the calling thread (the "game thread"):
// RuntimeInit() (host version of sdk_runtime.c) and then the game's main(),
// renamed to game_main by the build and reached through host_call_game_main.
// The game leaves through longjmp:
//   ResetToBootLoader()       -> DeviceTerm(), exit code 0
//   panic(), FatalErrorStop() -> plat_log(message), exit code 1
// If game_main() returns (on the device the CPU would idle in an endless
// loop), DeviceTerm() is called and the exit code is 0.

#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>

#include "host_internal.h"
#include "_sdk/inc/sdk_clocks.h"		// CurrentFreq, CLK_SYS
#include "_sdk/inc/sdk_rosc.h"			// RoscRand
#include "_sdk/inc/sdk_systick.h"		// SysTime, UnixTime, CurTimeMs
#include "_sdk/inc/sdk_watchdog.h"		// ResetToBootLoader
#include "_lib/inc/lib_rand.h"			// RandSeed, RandInit
#include "_lib/inc/lib_alarm.h"			// AlarmInit
#include "_lib/inc/lib_malloc.h"		// MemInit
#include "_devices/picopad/picopad_init.h"	// DeviceInit, DeviceTerm

#define JMP_RESET	1	// longjmp code: ResetToBootLoader
#define JMP_FATAL	2	// longjmp code: panic / fatal error
#define JMP_EXIT	3	// longjmp code: host_exit (code in ExitCode)

#define AUDIO_RATE_DEF	44100	// CLK_SYS base when audio is off

static jmp_buf GameJmp;
static volatile int GameJmpValid;	// 1 = GameJmp is set (inside host_run_game)
static volatile int ExitCode;		// exit code of host_exit
static u32 RoscState;			// state of the deterministic RoscRand

// ROSC random generator replacement: deterministic sequence from
// host_config.seed (splitmix32), restarted by every RuntimeInit()
u32 RoscRand(void)
{
	u32 z = (RoscState += 0x9E3779B9UL);
	z = (z ^ (z >> 16)) * 0x85EBCA6BUL;
	z = (z ^ (z >> 13)) * 0xC2B2AE35UL;
	return z ^ (z >> 16);
}

// host version of RuntimeInit (sdk_runtime.c): only the parts that matter
// without hardware, in the original order
void RuntimeInit(void)
{
	const host_config* cfg = host_cfg();

	// ROM function pointers (sdk_stubs.c)
	RomFncInit();

	// system clock as ClockInit() would report it; PWMSndInit derives the
	// sample rate from it (CLK_SYS / 2048)
	CurrentFreq[CLK_SYS] = 2048UL * (u32)(cfg->audio_rate ? cfg->audio_rate : AUDIO_RATE_DEF);

	// random generator: same initial seeds as lib_rand.c, then randomized
	// from the deterministic RoscRand as in the original RuntimeInit
	RoscState = cfg->seed;
	RandSeed[0] = 123456789123456789ULL;
	RandSeed[1] = 198765432198765432ULL;
	RandInit();
	RandSeed[1] += RandSeed[0];
	RandSeed[0] += RoscRand();

	AlarmInit();

	// SysTickInit(): reset system time
	SysTime = 0;
	UnixTime = 0;
	CurTimeMs = 0;

	MemInit();

	DeviceInit();
}

int host_run_game(void)
{
	volatile int code;

	host_set_game_thread(1);
	host_clock_reset();
	host_pump_reset();
	host_audio_reset();
	host_pump_clear_exit();

	code = setjmp(GameJmp);
	if (code == 0)
	{
		GameJmpValid = 1;
		RuntimeInit();
		host_call_game_main();
		DeviceTerm();
		code = 0;
	}
	else if (code == JMP_EXIT)
		code = ExitCode;
	else
		code = (code == JMP_RESET) ? 0 : 1;

	GameJmpValid = 0;
	host_pump_reset();
	host_set_game_thread(0);
	return code;
}

// end the game from the platform (plat_tick): exit code 'code'
void host_exit(int code)
{
	if (!GameJmpValid || !host_is_game_thread()) host_fatal("host_exit outside of the game");
	ExitCode = code;
	longjmp(GameJmp, JMP_EXIT);
}

void ResetToBootLoader(void)
{
	DeviceTerm();
	if (!GameJmpValid || !host_is_game_thread()) host_fatal("ResetToBootLoader outside of the game");
	longjmp(GameJmp, JMP_RESET);
}

void host_fatal(const char* msg)
{
	plat_log(msg);
	if (GameJmpValid && host_is_game_thread()) longjmp(GameJmp, JMP_FATAL);
	__builtin_trap();	// not on the game thread: cannot unwind
}

void panic(const char* fmt, ...)
{
	char buf[256];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	host_fatal(buf);
}

void FatalErrorStop(void)
{
	host_fatal("FatalErrorStop");
}
