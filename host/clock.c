// ****************************************************************************
//
//                         Host runtime - emulated clock
//
// ****************************************************************************
// Device time in microseconds since host_run_game() started.
//  Real clock (virtual_clock = 0): plat_now_us() - start.
//  Virtual clock (virtual_clock = 1): a counter advanced only by WaitUs/WaitMs
//   (and by the display transfer model through host_wait_us). Code that polls
//   Time() in a loop without waiting does not advance it by itself; the pump's
//   idle guard (PUMP_IDLE_CALLS pumps without progress) and the stall watchdog
//   step it to the next SysTick (host_clock_advance_to_next_tick, pump.c).
// Real-clock waits keep an absolute deadline (WaitDeadline): a wait sets it to
// the emulated time at entry plus the requested time and sleeps only while it
// is at least WAIT_MIN_SLEEP_US ahead of real time; a shorter remainder is not
// slept but carried into the next wait. An OS sleep costs ~0.5 ms at least, so
// sleeping for each short wait (a 16x16 DispUpdate = 170 us) would make it ~3x
// slower. Emulated time is max(real time, WaitDeadline), so after WaitUs(n)
// Time() has always advanced by at least n (as on the device), and time the
// game computed between waits is never forgiven. A sleep overshoot is absorbed
// (real time is already past the deadline), so in the long run emulated time
// tracks real time.
// Implements the SDK timer functions that are out-of-line in sdk_timer.c
// (Time64, Time64Fast, WaitUs, WaitMs); Time() is inline -> host_time_us32().

#include "host_internal.h"

static host_config HostCfg = { 0, 1, 1, 0 };	// virtual_clock, fast_display, seed, audio_rate
static unsigned long long RealStart;		// plat_now_us() at host_clock_reset()
// VirtNow and WaitDeadline are read by the watchdog thread too (host_now_us
// in its pump step) and VirtNow is also raised by it: 64-bit values on i686,
// so every access is atomic (never torn), VirtNow only moves forward
static unsigned long long VirtNow;		// virtual clock counter [us] (atomic)
static unsigned long long WaitDeadline;		// real clock: end of the requested waits [us] (atomic, game thread writes)

#define WAIT_MIN_SLEEP_US	1000ULL		// sleep only if the deadline is this far ahead
#define WAIT_MAX_SLEEP_US	1000ULL		// longest single sleep (pump in between)

void host_configure(const host_config* cfg)
{
	HostCfg = *cfg;
}

const host_config* host_cfg(void)
{
	return &HostCfg;
}

void host_clock_reset(void)
{
	RealStart = plat_now_us();
	__atomic_store_n(&VirtNow, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&WaitDeadline, 0, __ATOMIC_SEQ_CST);
}

void host_clock_advance_to_next_tick(unsigned long long tick_us)
{
	if (!HostCfg.virtual_clock) return;
	unsigned long long cur = __atomic_load_n(&VirtNow, __ATOMIC_RELAXED);
	while (cur < tick_us && !__atomic_compare_exchange_n(&VirtNow, &cur, tick_us, 0,
			__ATOMIC_RELAXED, __ATOMIC_RELAXED)) { }
}

// real time since host_clock_reset() [us]
static unsigned long long real_now_us(void)
{
	return plat_now_us() - RealStart;
}

unsigned long long host_now_us(void)
{
	if (HostCfg.virtual_clock) return __atomic_load_n(&VirtNow, __ATOMIC_RELAXED);
	unsigned long long real = real_now_us();
	unsigned long long dl = __atomic_load_n(&WaitDeadline, __ATOMIC_RELAXED);
	return (real > dl) ? real : dl;
}

// Time() of the SDK; pumps, so loops polling Time() still get SysTick and,
// on the real clock, the busy-wait protection of the pump
u32 host_time_us32(void)
{
	host_pump();
	return (u32)host_now_us();
}

void host_wait_us(unsigned long long us)
{
	unsigned long long deadline;

	// a real wait restarts the idle count of the pump (virtual clock guard)
	if (us > 0 && host_is_game_thread()) host_pump_idle_reset();

	if (HostCfg.virtual_clock)
	{
		// advance in steps of at most 5000 us (one SysTick period) and pump
		// after each step, so SysTick handlers see evenly spaced Time()
		do
		{
			unsigned long long step = (us > 5000) ? 5000 : us;
			__atomic_add_fetch(&VirtNow, step, __ATOMIC_RELAXED);
			us -= step;
			host_pump();
		} while (us > 0);
		return;
	}

	// host_now_us() >= WaitDeadline always, so assigning (not max'ing) keeps
	// the deadline - and with it the emulated time - monotonic
	deadline = host_now_us() + us;
	__atomic_store_n(&WaitDeadline, deadline, __ATOMIC_RELAXED);
	for (;;)
	{
		unsigned long long now = real_now_us();
		if (deadline < now + WAIT_MIN_SLEEP_US) break;
		unsigned long long step = deadline - now;
		if (step > WAIT_MAX_SLEEP_US) step = WAIT_MAX_SLEEP_US;
		plat_sleep_us(step);
		host_pump();
	}
	host_pump();
}

u64 Time64(void)
{
	host_pump();
	return (u64)host_now_us();
}

u64 Time64Fast(void)
{
	return Time64();
}

void WaitUs(u32 us)
{
	host_wait_us(us);
}

void WaitMs(int ms)
{
	if (ms > 0) host_wait_us((unsigned long long)ms * 1000);
	else host_pump();
}
