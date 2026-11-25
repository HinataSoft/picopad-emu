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
//
// Stall protection:
//  - virtual clock: PUMP_IDLE_CALLS top-level pumps without any change of
//    host_now_us() (a loop polling Time() or calling dmb()) advance the
//    virtual clock to the next SysTick (host_clock_advance_to_next_tick), so
//    such loops end; deterministic. A wait (host_wait_us) restarts the count;
//  - watchdog: host_watchdog() is called by the platform from its own thread
//    every ~20 ms. A game loop without any SDK call (while (PlayingSound()) {})
//    never pumps, so SysTick and the mixer would stop forever. Every game-thread
//    pump increments PumpSeq (no real-time read on the game thread); when the
//    watchdog sees PumpSeq unchanged for STALL_US of real time, it takes the
//    pump lock and the IRQ mutex without waiting (busy = skip this call) and
//    runs one pump step itself: virtual clock - advance to the next tick, run
//    it and the mixer; real clock - run the ticks due by real time and the
//    mixer. plat_tick >= 0 there becomes host_request_exit (no longjmp off the
//    game thread); once an exit is pending the watchdog does nothing more.
//    A fatal error inside the step (panic/FatalErrorStop from SysTick, alarm
//    or mixer code) cannot longjmp to host_run_game from this thread: it
//    unwinds the step instead (WdJmp, host_watchdog_unwind), releases the
//    locks and requests exit code 1 (host_pending_exit tells the platform).
//    Its own step does not count in PumpSeq; the episode lasts
//    until PumpSeq moves again, with one step per call; the first step of an
//    episode is logged ("stall: ..."). The watchdog acts only while a game is
//    inside host_run_game (host_pump_activate).
//
// Pump lock (InPump, CAS): PUMP_FREE / PUMP_GAME / PUMP_WATCHDOG. The game
// thread waits for a watchdog step to finish (it is short and never waits for
// the game), so no game pump is ever skipped (determinism); a nested
// game-thread pump (InPump == PUMP_GAME) returns at once. The watchdog only
// tries. Only the game thread sets PUMP_GAME and only the watchdog sets
// PUMP_WATCHDOG; each owner releases its own value.
//
// Thread ownership of the shared state:
//  - GameThread, IrqDepth, IrqTag: per thread (__thread);
//  - IrqOwner: any thread, CAS;
//  - InPump: CAS / atomic store (see above);
//  - LastTick: under the pump lock (game thread or watchdog), and in
//    host_pump_reset on the game thread while the watchdog is inactive;
//  - SpinStart, SpinCalls, IdleNow, IdleCalls: game thread only;
//  - PumpSeq, RunId: written by the game thread only, read by the watchdog
//    (atomic); GameActive: written by the game thread, read by the watchdog;
//  - StallCount: incremented by the watchdog under the pump lock, reset by
//    the game thread at run start, read by anyone (atomic);
//  - WdBusy (CAS) guards the watchdog's own state (WdRun, WdSeq, WdTime,
//    WdEpisode), so overlapping host_watchdog calls are harmless (the later
//    one returns).

#include <setjmp.h>

#include "host_internal.h"
#include "_sdk/inc/sdk_systick.h"		// SysTime, UnixTime, CurTimeMs, SYSTICK_MS
#include "_lib/inc/lib_alarm.h"			// AlarmUpdate
#include "_devices/picopad/picopad_key.h"	// KeyScan

#define PUMP_TICK_US		(SYSTICK_MS*1000ULL)	// SysTick period [us]
#define PUMP_SPIN_CALLS		1000			// pumps without 1 ms elapsed = busy wait
#define PUMP_SPIN_WINDOW_US	1000ULL
#define PUMP_SPIN_SLEEP_US	200ULL
#define PUMP_IDLE_CALLS		1000			// virtual clock: pumps without time progress -> next tick
#define STALL_US		100000ULL		// watchdog: real time without a pump = stall

#define PUMP_FREE		0			// InPump values
#define PUMP_GAME		1
#define PUMP_WATCHDOG		2

#define WD_FATAL		(-2)			// wd_step result: fatal error inside the step

// system time (originally sdk_systick.c)
volatile u32 SysTime;		// [ms], incremented every SYSTICK_MS
volatile u32 UnixTime;		// current date and time in Unix format
volatile s16 CurTimeMs;		// current time in [ms] 0..999

static __thread int GameThread;		// 1 = this thread runs host_run_game
static __thread int IrqDepth;		// IRQ lock nesting of this thread
static __thread char IrqTag;		// address = identity of this thread
static void* volatile IrqOwner;		// thread holding the IRQ mutex, NULL = free

static volatile int PendingExit = -1;	// exit code requested by host_request_exit, <0 = none
static int InPump;			// pump lock PUMP_FREE/GAME/WATCHDOG (atomic)
static unsigned long long LastTick;	// emulated time of the last SysTick [us] (pump lock)
static unsigned long long SpinStart;	// start of the spin window [us] (game thread)
static int SpinCalls;			// pumps in the current spin window (game thread)
static unsigned long long IdleNow;	// host_now_us() when IdleCalls restarted (game thread)
static int IdleCalls;			// pumps since then without time progress (game thread)

static unsigned int PumpSeq;		// game-thread pump counter (atomic, game thread writes)
static unsigned int RunId;		// host_run_game counter (atomic, game thread writes)
static int GameActive;			// 1 = a game is inside host_run_game (atomic)
static unsigned int StallCount;		// watchdog steps since the run start (atomic)

static int WdBusy;			// host_watchdog in progress (CAS), guards Wd*
static unsigned int WdRun;		// RunId seen by the watchdog
static unsigned int WdSeq;		// PumpSeq seen by the watchdog
static unsigned long long WdTime;	// plat_now_us() when WdSeq/WdRun last changed
static int WdEpisode;			// 1 = inside a stall episode (already stepped)
static __thread jmp_buf* WdJmp;		// set while this thread runs a watchdog step

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

// lock the IRQ mutex without waiting; 1 = locked (unlock with host_irq_unlock)
static int irq_trylock(void)
{
	void* expect = NULL;
	if (IrqDepth > 0)
	{
		IrqDepth++;
		return 1;
	}
	if (!__atomic_compare_exchange_n(&IrqOwner, &expect, (void*)&IrqTag, 0,
			__ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return 0;
	IrqDepth = 1;
	return 1;
}

void host_request_exit(int code)
{
	__atomic_store_n(&PendingExit, code, __ATOMIC_SEQ_CST);
}

int host_pending_exit(void)
{
	return __atomic_load_n(&PendingExit, __ATOMIC_SEQ_CST);
}

void host_pump_clear_exit(void)
{
	__atomic_store_n(&PendingExit, -1, __ATOMIC_SEQ_CST);
}

void host_pump_idle_reset(void)
{
	IdleCalls = 0;
}

void host_pump_activate(int on)
{
	if (on)
	{
		__atomic_store_n(&StallCount, 0, __ATOMIC_SEQ_CST);
		__atomic_store_n(&RunId, RunId + 1, __ATOMIC_SEQ_CST);
		__atomic_store_n(&GameActive, 1, __ATOMIC_SEQ_CST);
		return;
	}
	// the watchdog re-checks GameActive after taking the pump lock (all
	// seq_cst): either it sees 0 and backs off, or we see its lock and wait
	__atomic_store_n(&GameActive, 0, __ATOMIC_SEQ_CST);
	while (__atomic_load_n(&InPump, __ATOMIC_SEQ_CST) == PUMP_WATCHDOG)
		__builtin_ia32_pause();
}

unsigned int host_stall_count(void)
{
	return __atomic_load_n(&StallCount, __ATOMIC_SEQ_CST);
}

void host_pump_reset(void)
{
	// release the pump lock only if this (game) thread holds it (longjmp out
	// of host_pump); never clobber a watchdog that is backing off
	int expect = PUMP_GAME;
	__atomic_compare_exchange_n(&InPump, &expect, PUMP_FREE, 0,
		__ATOMIC_RELEASE, __ATOMIC_RELAXED);
	LastTick = host_now_us();
	SpinStart = LastTick;
	SpinCalls = 0;
	IdleNow = LastTick;
	IdleCalls = 0;
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

// pump body, under the pump lock (game thread or watchdog): the SysTicks due
// by 'now', plat_tick before each, then the mixer. Returns the exit code of
// plat_tick (>= 0; the remaining ticks and the mixer are skipped) or -1.
static int pump_step(unsigned long long now)
{
	while (now >= LastTick + PUMP_TICK_US)
	{
		LastTick += PUMP_TICK_US;
		int rc = plat_tick(LastTick);
		if (rc >= 0) return rc;
		SysTickHandler();
	}
	host_audio_pump();
	return -1;
}

void host_pump(void)
{
	if (!GameThread) return;
	// only this thread writes PumpSeq: plain increment, published atomically
	__atomic_store_n(&PumpSeq, PumpSeq + 1, __ATOMIC_RELAXED);
	if (IrqDepth > 0) return;
	if (__atomic_load_n(&InPump, __ATOMIC_RELAXED) == PUMP_GAME) return;	// nested

	int pending = __atomic_load_n(&PendingExit, __ATOMIC_SEQ_CST);
	if (pending >= 0) host_exit(pending);	// requested by host_request_exit

	// take the pump lock; besides us only a (short) watchdog step holds it
	int expect = PUMP_FREE;
	while (!__atomic_compare_exchange_n(&InPump, &expect, PUMP_GAME, 0,
			__ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
	{
		expect = PUMP_FREE;
		__builtin_ia32_pause();
	}

	const int virt = host_cfg()->virtual_clock;
	unsigned long long now = host_now_us();
	if (virt)
	{
		// polling without waiting: after PUMP_IDLE_CALLS pumps without time
		// progress, step to the next tick (deterministic)
		if (now != IdleNow)
		{
			IdleNow = now;
			IdleCalls = 0;
		}
		else if (++IdleCalls >= PUMP_IDLE_CALLS)
		{
			host_clock_advance_to_next_tick(LastTick + PUMP_TICK_US);
			now = host_now_us();
			IdleNow = now;
			IdleCalls = 0;
		}
	}

	int rc = pump_step(now);
	if (rc >= 0) host_exit(rc);	// the lock is released by host_pump_reset

	__atomic_store_n(&InPump, PUMP_FREE, __ATOMIC_RELEASE);

	if (!virt)
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
}

// host_fatal off the game thread: inside a watchdog step, leave the step
// (longjmp to wd_step); otherwise return
void host_watchdog_unwind(void)
{
	jmp_buf* jb = WdJmp;
	if (jb == NULL) return;
	WdJmp = NULL;
	longjmp(*jb, 1);
}

// watchdog step under the pump lock and the IRQ mutex: plat_tick result,
// -1 or WD_FATAL (fatal error inside; the IRQ nesting it left is undone)
static int wd_step(void)
{
	jmp_buf jb;
	int rc;
	if (setjmp(jb) != 0)
	{
		// unwound from host_fatal: SDK code may have left IRQ_LOCK nested
		if (IrqDepth > 1) IrqDepth = 1;
		return WD_FATAL;
	}
	WdJmp = &jb;
	if (host_cfg()->virtual_clock)
		host_clock_advance_to_next_tick(LastTick + PUMP_TICK_US);
	rc = pump_step(host_now_us());
	WdJmp = NULL;
	return rc;
}

void host_watchdog(void)
{
	int expect = 0, logit = 0;

	if (!__atomic_load_n(&GameActive, __ATOMIC_SEQ_CST)) return;
	// an exit is pending: no further steps (the game ends at its next pump,
	// or the platform ends a game that never pumps again)
	if (__atomic_load_n(&PendingExit, __ATOMIC_SEQ_CST) >= 0) return;
	if (!__atomic_compare_exchange_n(&WdBusy, &expect, 1, 0,
			__ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) return;

	unsigned int run = __atomic_load_n(&RunId, __ATOMIC_SEQ_CST);
	unsigned int seq = __atomic_load_n(&PumpSeq, __ATOMIC_SEQ_CST);
	unsigned long long now = plat_now_us();

	if (run != WdRun || seq != WdSeq)
	{
		// new run, or the game pumped since the last look: no stall, episode over
		WdRun = run;
		WdSeq = seq;
		WdTime = now;
		WdEpisode = 0;
	}
	else if (WdEpisode || now - WdTime >= STALL_US)
	{
		expect = PUMP_FREE;
		if (__atomic_compare_exchange_n(&InPump, &expect, PUMP_WATCHDOG, 0,
				__ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{
			// still the same run, the game still stalled, and the game
			// thread not inside IRQ_LOCK (else skip this call)
			if (__atomic_load_n(&GameActive, __ATOMIC_SEQ_CST) &&
				__atomic_load_n(&RunId, __ATOMIC_SEQ_CST) == run &&
				__atomic_load_n(&PumpSeq, __ATOMIC_SEQ_CST) == seq &&
				irq_trylock())
			{
				int rc = wd_step();
				if (rc == WD_FATAL) host_request_exit(1);	// message already logged
				else if (rc >= 0) host_request_exit(rc);	// cannot longjmp here
				host_irq_unlock();
				__atomic_add_fetch(&StallCount, 1, __ATOMIC_SEQ_CST);
				logit = !WdEpisode;
				WdEpisode = 1;
			}
			__atomic_store_n(&InPump, PUMP_FREE, __ATOMIC_RELEASE);
		}
	}

	__atomic_store_n(&WdBusy, 0, __ATOMIC_RELEASE);
	if (logit) plat_log("stall: game loop without SDK calls, advancing time");
}
