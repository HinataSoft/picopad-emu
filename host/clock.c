// ****************************************************************************
//
//                         Host runtime - emulated clock
//
// ****************************************************************************
// Device time in microseconds since host_run_game() started.
//  Real clock (virtual_clock = 0): plat_now_us() - start.
//  Virtual clock (virtual_clock = 1): a counter advanced only by WaitUs/WaitMs
//   (and by the display transfer model through host_wait_us). Code that polls
//   Time() in a loop without waiting does not advance it.
// Implements the SDK timer functions that are out-of-line in sdk_timer.c
// (Time64, Time64Fast, WaitUs, WaitMs); Time() is inline -> host_time_us32().

#include "host_internal.h"

static host_config HostCfg = { 0, 1, 1, 0 };	// virtual_clock, fast_display, seed, audio_rate
static unsigned long long RealStart;		// plat_now_us() at host_clock_reset()
static unsigned long long VirtNow;		// virtual clock counter [us]

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
	VirtNow = 0;
}

unsigned long long host_now_us(void)
{
	if (HostCfg.virtual_clock) return VirtNow;
	return plat_now_us() - RealStart;
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
	if (HostCfg.virtual_clock)
	{
		// advance in steps of at most 5000 us (one SysTick period) and pump
		// after each step, so SysTick handlers see evenly spaced Time()
		do
		{
			unsigned long long step = (us > 5000) ? 5000 : us;
			VirtNow += step;
			us -= step;
			host_pump();
		} while (us > 0);
		return;
	}

	unsigned long long end = host_now_us() + us;
	for (;;)
	{
		unsigned long long now = host_now_us();
		if (now >= end) break;
		unsigned long long step = end - now;
		if (step > 1000) step = 1000;
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
