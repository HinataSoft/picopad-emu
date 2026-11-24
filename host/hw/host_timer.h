// ****************************************************************************
//
//                    Host replacement of _sdk/inc/sdk_timer.h
//
// ****************************************************************************
// Declarations, constants and macros are taken from PicoLibSDK
// _sdk/inc/sdk_timer.h (Copyright (c) 2023 Miroslav Nemecek, freely available
// for any purpose). Time() reads the emulator clock (host_time_us32()),
// alarm register access is replaced by no-ops. RP2040 only.
// Out-of-line functions (Time64, WaitUs, WaitMs, Alarm*...) keep their
// original prototypes and are implemented by the emulator.

#ifndef _HOST_TIMER_H
#define _HOST_TIMER_H

#if USE_TIMER	// use Timer with alarm (sdk_timer.c, sdk_timer.h)

#include "_sdk/sdk_addressmap.h"	// Register address offsets
#include "host_irq.h"
#include "_sdk/inc/sdk_sio.h"		// SIO registers

#if USE_ORIGSDK		// include interface of original SDK
#include "_sdk/inc/orig_rp2040/orig_timer.h"	// constants of original SDK
#endif

#ifdef __cplusplus
extern "C" {
#endif

// emulator time in [us], 32-bit wrapping (host function, host/*.c)
u32 host_time_us32(void);

#define ALARMS_NUM	4		// number of alarms per timer
#define NUM_TIMERS	ALARMS_NUM	// number of timer alarms

// timer hardware registers (layout kept for compatibility, never accessed on host)
typedef struct {
	io32	timehw;			// 0x00: Write to bits 63:32 of time
	io32	timelw;			// 0x04: Write to bits 31:0 of time
	io32	timehr;			// 0x08: Read from bits 63:32 of time
	io32	timelr;			// 0x0C: Read from bits 31:0 of time
	io32	alarm[ALARMS_NUM];	// 0x10: Arm alarm, and configure the time it will fire
	io32	armed;			// 0x20: Indicates the armed/disarmed status of each alarm
	io32	timerawh;		// 0x24: Raw read from bits 63:32 of time
	io32	timerawl;		// 0x28: Raw read from bits 31:0 of time
	io32	dbgpause;		// 0x2C: debug pause
	io32	pause;			// 0x30: Set high to pause the timer
	io32	intr;			// 0x34: Raw Interrupts
	io32	inte;			// 0x38: Interrupt Enable
	io32	intf;			// 0x3C: Interrupt Force
	io32	ints;			// 0x40: Interrupt status after masking & forcing
} timer_hw_t;

#define timer_hw ((timer_hw_t*)TIMER_BASE)
#define timer0_hw ((timer_hw_t *)TIMER_BASE)

#define ALARM_IRQ(alarm) (IRQ_TIMER_0 + (alarm))

// claimed alarms (defined by the emulator)
extern Bool AlarmClaimed[ALARMS_NUM];

// get low 32 bits of system time in [us]
static inline u32 Time(void) { return host_time_us32(); }

// get system time in [ms] (time / 1024)
static inline u16 TimeMs(void) { return (u16)(Time()>>10); }

// get 64-bit system time in [us]
u64 Time64Fast(void);
u64 Time64(void);

// wait for delay in [us] / [ms]
void WaitUs(u32 us);
void WaitMs(int ms);

// claim/unclaim alarm
static inline void AlarmClaim(int alarm) { AlarmClaimed[alarm] = True; }
static inline void AlarmUnclaim(int alarm) { AlarmClaimed[alarm] = False; }
static inline Bool AlarmIsClaimed(int alarm) { return AlarmClaimed[alarm]; }
s8 AlarmClaimFree(void);

#if USE_IRQ	// use IRQ interrupts (sdk_irq.c, sdk_irq.h)
void AlarmStart(int alarm, irq_handler_t handler, u32 time);
static inline void AlarmForce(int alarm) { (void)alarm; }
static inline void AlarmUnforce(int alarm) { (void)alarm; }
static inline Bool AlarmIsForced(int alarm) { (void)alarm; return False; }
static inline void AlarmAck(int alarm) { (void)alarm; }
static inline Bool AlarmIsPending(int alarm) { (void)alarm; return False; }
void AlarmRestart(int alarm, u32 time);
void AlarmStop(int alarm);
#endif // USE_IRQ

// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original SDK

typedef void (*hardware_alarm_callback_t)(uint alarm_num);

static inline uint harware_alarm_irq_number(uint alarm_num) { return ALARM_IRQ(alarm_num); }
static inline u64 to_us_since_boot(absolute_time_t t) { return (u64)t; }
static inline void update_us_since_boot(absolute_time_t *t, u64 us_since_boot) { *t = us_since_boot; }
static inline absolute_time_t from_us_since_boot(u64 us_since_boot) { return (absolute_time_t)us_since_boot; }
static inline absolute_time_t get_absolute_time(void) { return (absolute_time_t)Time64(); }
static inline u32 us_to_ms(u64 us) { return ((us >> 32) != 0) ? (u32)(us/1000) : ((u32)us)/1000; }
static inline u32 to_ms_since_boot(absolute_time_t t) { return us_to_ms((u64)t); }
static inline absolute_time_t delayed_by_us(const absolute_time_t t, u64 us) { return (absolute_time_t)((u64)t + us); }
static inline absolute_time_t delayed_by_ms(const absolute_time_t t, u32 ms) { return delayed_by_us(t, ms*1000ull); }
static inline absolute_time_t make_timeout_time_us(u64 us) { return delayed_by_us(get_absolute_time(), us); }
static inline absolute_time_t make_timeout_time_ms(u32 ms) { return delayed_by_ms(get_absolute_time(), ms); }
static inline s64 absolute_time_diff_us(absolute_time_t from, absolute_time_t to) { return (s64)((u64)to - (u64)from); }
static inline absolute_time_t absolute_time_min(absolute_time_t a, absolute_time_t b) { return ((u64)a < (u64)b) ? a : b; }
static inline void sleep_until(absolute_time_t target) { s64 dif = (u64)target - Time64(); if (dif > 0) WaitUs((u32)dif); }
static inline void sleep_us(u64 us) { WaitUs((u32)us); }
static inline void sleep_ms(u32 ms) { WaitMs(ms); }
static inline u32 time_us_32(void) { return Time(); }
static inline u64 time_us_64(void) { return Time64(); }
static inline void busy_wait_us_32(u32 delay_us) { WaitUs(delay_us); }
static inline void busy_wait_us(u64 delay_us) { WaitUs((u32)delay_us); }
static inline void busy_wait_ms(u32 delay_ms) { WaitMs(delay_ms); }
static inline void busy_wait_until(absolute_time_t t)
{
	u64 now = Time64();
	if ((u64)t > now) WaitUs((u32)((u64)t - now));
}
static inline bool time_reached(absolute_time_t t) { return Time64() >= (u64)t; }
static inline bool best_effort_wfe_or_timeout(absolute_time_t timeout_timestamp) { return time_reached(timeout_timestamp); }
static inline void hardware_alarm_claim(uint alarm_num) { AlarmClaim(alarm_num); }
static inline int hardware_alarm_claim_unused(bool required) { (void)required; return AlarmClaimFree(); }
static inline void hardware_alarm_unclaim(uint alarm_num) { AlarmUnclaim(alarm_num); }
static inline bool hardware_alarm_is_claimed(uint alarm_num) { return AlarmIsClaimed(alarm_num); }
void hardware_alarm_set_callback(uint alarm_num, hardware_alarm_callback_t callback);
bool hardware_alarm_set_target(uint alarm_num, absolute_time_t target);
void hardware_alarm_cancel(uint alarm_num);
void hardware_alarm_force_irq(uint alarm_num);

typedef s32 alarm_id_t;
typedef struct alarm_pool alarm_pool_t;
typedef s64 (*alarm_callback_t)(alarm_id_t id, void *user_data);

static inline void alarm_pool_init_default(void) {}
static inline alarm_pool_t* alarm_pool_get_default(void) { return (alarm_pool_t*)NULL; }
static inline alarm_pool_t* alarm_pool_create(uint hardware_alarm_num, uint max_timers) { (void)hardware_alarm_num; (void)max_timers; return (alarm_pool_t*)NULL; }
static inline alarm_pool_t* alarm_pool_create_with_unused_hardware_alarm(uint max_timers) { (void)max_timers; return (alarm_pool_t*)NULL; }
static inline uint alarm_pool_hardware_alarm_num(alarm_pool_t *pool) { (void)pool; return 0; }
static inline uint alarm_pool_core_num(alarm_pool_t *pool) { (void)pool; return 0; }
static inline void alarm_pool_destroy(alarm_pool_t *pool) { (void)pool; }
alarm_id_t alarm_pool_add_alarm_at(alarm_pool_t *pool, absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past);
static inline alarm_id_t alarm_pool_add_alarm_in_us(alarm_pool_t *pool, u64 us, alarm_callback_t callback, void *user_data, bool fire_if_past)
	{ return alarm_pool_add_alarm_at(pool, delayed_by_us(get_absolute_time(), us), callback, user_data, fire_if_past); }
static inline alarm_id_t alarm_pool_add_alarm_in_ms(alarm_pool_t *pool, u32 ms, alarm_callback_t callback, void *user_data, bool fire_if_past)
	{ return alarm_pool_add_alarm_at(pool, delayed_by_ms(get_absolute_time(), ms), callback, user_data, fire_if_past); }
bool alarm_pool_cancel_alarm(alarm_pool_t *pool, alarm_id_t alarm_id);
static inline alarm_id_t add_alarm_at(absolute_time_t time, alarm_callback_t callback, void *user_data, bool fire_if_past)
	{ return alarm_pool_add_alarm_at(alarm_pool_get_default(), time, callback, user_data, fire_if_past); }
static inline alarm_id_t add_alarm_in_us(u64 us, alarm_callback_t callback, void *user_data, bool fire_if_past)
	{ return alarm_pool_add_alarm_in_us(alarm_pool_get_default(), us, callback, user_data, fire_if_past); }
static inline alarm_id_t add_alarm_in_ms(uint32_t ms, alarm_callback_t callback, void *user_data, bool fire_if_past)
	{ return alarm_pool_add_alarm_in_ms(alarm_pool_get_default(), ms, callback, user_data, fire_if_past); }
static inline bool cancel_alarm(alarm_id_t alarm_id)
	{ return alarm_pool_cancel_alarm(alarm_pool_get_default(), alarm_id); }

typedef struct repeating_timer repeating_timer_t;
typedef bool (*repeating_timer_callback_t)(repeating_timer_t *rt);

struct repeating_timer
{
	s64				delay_us;
	alarm_pool_t*			pool;
	alarm_id_t			alarm_id;
	repeating_timer_callback_t	callback;
	void*				user_data;
};

bool alarm_pool_add_repeating_timer_us(alarm_pool_t *pool, s64 delay_us, repeating_timer_callback_t callback, void *user_data, repeating_timer_t *out);
static inline bool add_repeating_timer_us(int64_t delay_us, repeating_timer_callback_t callback, void *user_data, repeating_timer_t *out)
	{ return alarm_pool_add_repeating_timer_us(alarm_pool_get_default(), delay_us, callback, user_data, out); }
static inline bool add_repeating_timer_ms(s32 delay_ms, repeating_timer_callback_t callback, void *user_data, repeating_timer_t *out)
	{ return alarm_pool_add_repeating_timer_us(alarm_pool_get_default(), delay_ms * (s64)1000, callback, user_data, out); }
bool cancel_repeating_timer(repeating_timer_t *timer);

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // USE_TIMER

#endif // _HOST_TIMER_H
