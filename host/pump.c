// ****************************************************************************
//
//                  Host runtime - SysTick pump and IRQ mutex
//
// ****************************************************************************
// The device runs SysTick_Handler every SYSTICK_MS (5 ms) from an interrupt.
// The host has no interrupts: host_pump() is called from the places where the
// game gives the CPU a chance (dmb/wfe/BusyWaitCycles, GPIO_In, Time(),
// WaitUs/WaitMs, USB stubs) and runs the handler as many times as 5 ms
// periods have elapsed on the emulated clock (host_now_us()).
//  - runs only on the game thread (other threads, e.g. audio, are ignored)
//  - not reentrant: handlers (AlarmUpdate, KeyScan) call dmb()/GPIO_In(),
//    which call host_pump() again; the nested call returns immediately
//  - not inside a critical section (IRQ_LOCK held by the game thread), like
//    a masked interrupt on the device
//  - real clock: if called more than 1000 times without 1 ms elapsing, the
//    game is busy-waiting; sleep 200 us to spare the host core
//
// IRQ mutex: host_irq_lock/unlock (LockIRQ, IRQ_LOCK, di/ei) is a recursive
// lock shared by all threads; it is shared with any other thread that needs
// to serialize against IRQ_LOCK sections (the audio thread does not: it only
// drains a ring buffer, see audio.c). The nesting depth is per thread; an
// unmatched unlock (ei() without di()) is ignored.

#include "host_internal.h"
#include "_sdk/inc/sdk_systick.h"		// SysTime, UnixTime, CurTimeMs, SYSTICK_MS
#include "_lib/inc/lib_alarm.h"			// AlarmUpdate
#include "_devices/picopad/picopad_key.h"	// KeyScan

#define PUMP_TICK_US		(SYSTICK_MS*1000ULL)	// SysTick period [us]
#define PUMP_SPIN_CALLS		1000			// pumps without 1 ms elapsed = busy wait
#define PUMP_SPIN_WINDOW_US	1000ULL
#define PUMP_SPIN_SLEEP_US	200ULL

// system time (originally sdk_systick.c)
volatile u32 SysTime;		// [ms], incremented every SYSTICK_MS
volatile u32 UnixTime;		// current date and time in Unix format
volatile s16 CurTimeMs;		// current time in [ms] 0..999

static __thread int GameThread;		// 1 = this thread runs host_run_game
static __thread int IrqDepth;		// IRQ lock nesting of this thread
static __thread char IrqTag;		// address = identity of this thread
static void* volatile IrqOwner;		// thread holding the IRQ mutex, NULL = free

static volatile int PendingExit = -1;	// exit code requested by host_request_exit, <0 = none
static int InPump;			// reentrancy flag (game thread only)
static unsigned long long LastTick;	// emulated time of the last SysTick [us]
static unsigned long long SpinStart;	// start of the spin window [us]
static int SpinCalls;			// pumps in the current spin window

int host_is_game_thread(void)
{
	return GameThread;
}

void host_set_game_thread(int on)
{
	GameThread = on;
}

void host_irq_lock(void)
{
	if (IrqDepth++ == 0)
	{
		void* me = &IrqTag;
		void* expect = NULL;
		while (!__atomic_compare_exchange_n(&IrqOwner, &expect, me, 0,
				__ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
		{
			expect = NULL;
			__builtin_ia32_pause();
		}
	}
}

void host_irq_unlock(void)
{
	if (IrqDepth <= 0) return;	// unmatched unlock
	if (--IrqDepth == 0) __atomic_store_n(&IrqOwner, NULL, __ATOMIC_RELEASE);
}

void host_request_exit(int code)
{
	__atomic_store_n(&PendingExit, code, __ATOMIC_SEQ_CST);
}

void host_pump_clear_exit(void)
{
	__atomic_store_n(&PendingExit, -1, __ATOMIC_SEQ_CST);
}

void host_pump_reset(void)
{
	InPump = 0;
	LastTick = host_now_us();
	SpinStart = LastTick;
	SpinCalls = 0;
	if (IrqDepth > 0)
	{
		IrqDepth = 0;
		if (IrqOwner == (void*)&IrqTag) __atomic_store_n(&IrqOwner, NULL, __ATOMIC_RELEASE);
	}
}

// one SysTick period (as SysTick_Handler of sdk_systick.c, core 0;
// UsbHostOnTime is skipped, USB host is never initialized on the host)
static void SysTickHandler(void)
{
	IRQ_LOCK;

	SysTime += SYSTICK_MS;

	u16 ms = CurTimeMs + SYSTICK_MS;
	if (ms >= 1000)
	{
		ms -= 1000;
		UnixTime++;
	}
	CurTimeMs = ms;

#if USE_ALARM
	AlarmUpdate();
#endif

	IRQ_UNLOCK;

#if SYSTICK_KEYSCAN
	KeyScan();
#endif
}

void host_pump(void)
{
	if (GameThread && !InPump && IrqDepth == 0)
	{
		int pending = __atomic_load_n(&PendingExit, __ATOMIC_SEQ_CST);
		if (pending >= 0) host_exit(pending);	// requested by host_request_exit
	}
	if (!GameThread || InPump || IrqDepth > 0) return;
	InPump = 1;

	unsigned long long now = host_now_us();
	while (now - LastTick >= PUMP_TICK_US)
	{
		LastTick += PUMP_TICK_US;
		int rc = plat_tick(LastTick);
		if (rc >= 0) host_exit(rc);
		SysTickHandler();
	}

	host_audio_pump();

	if (!host_cfg()->virtual_clock)
	{
		if (now - SpinStart >= PUMP_SPIN_WINDOW_US)
		{
			SpinStart = now;
			SpinCalls = 0;
		}
		else if (++SpinCalls > PUMP_SPIN_CALLS)
		{
			plat_sleep_us(PUMP_SPIN_SLEEP_US);
			SpinCalls = 0;
		}
	}

	InPump = 0;
}
