// ****************************************************************************
//
//                  Host replacement of _sdk/inc/sdk_spinlock.h
//
// ****************************************************************************
// Declarations, constants and macros are taken from PicoLibSDK
// _sdk/inc/sdk_spinlock.h (Copyright (c) 2023 Miroslav Nemecek, freely
// available for any purpose). The game runs on a single emulated core and
// interrupts are serialized by host_irq_lock(), so hardware spinlocks are
// no-ops on host (lock always succeeds, registers are never accessed).

#ifndef _HOST_SPINLOCK_H
#define _HOST_SPINLOCK_H

#if USE_SPINLOCK	// use Spinlock (sdk_spinlock.c, sdk_spinlock.h)

#include "_sdk/sdk_addressmap.h"	// Register address offsets
#include "host_cpu.h"
#include "host_timer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPINLOCK_NUM	32	// number of spinlocks

// spinlock register addresses (kept for compatibility, never accessed on host)
#define SPINLOCK_STATE	((volatile u32*)(SIO_BASE+0x5c)) // spinlock states
#define SPINLOCK(lock)	((volatile u32*)(SIO_BASE+(lock)*4+0x100)) // spinlock 0..31

// claimed spinlocks (defined by the emulator)
extern u32 SpinClaimed;		// claimed spinlocks
extern u8 SpinStriped;		// next striped spinlock

typedef volatile u32 spin_lock_t;

// get spinlock "hardware" from spinlock index (address only, not dereferenced)
static inline spin_lock_t* SpinGetHw(int lock) { return (spin_lock_t*)SPINLOCK(lock); }

// get spinlock index from spinlock hardware
static inline u8 SpinGetInx(const spin_lock_t* hw) { return (u8)(hw - SpinGetHw(0)); }

// === claim spinlock
u8 SpinNextStriped();
static inline void SpinClaim(int lock) { SpinClaimed |= BIT(lock); }
static inline void SpinClaimMask(u32 mask) { SpinClaimed |= mask; }
static inline void SpinUnclaim(int lock) { SpinClaimed &= ~BIT(lock); }
static inline void SpinUnclaimMask(u32 mask) { SpinClaimed &= ~mask; }
static inline Bool SpinIsClaimed(int lock) { return (SpinClaimed & BIT(lock)) != 0; }
s8 SpinClaimFree();

// === lock spinlock (no-ops on host)
static inline Bool SpinTryLock(int lock) { (void)lock; dmb(); return True; }
static inline Bool SpinTryLock_hw(spin_lock_t* hw) { (void)hw; dmb(); return True; }
static inline void SpinLock(int lock) { (void)lock; dmb(); }
static inline void SpinLock_hw(spin_lock_t* hw) { (void)hw; dmb(); }
static inline Bool SpinLockTimeout_hw(spin_lock_t* hw, u32 us) { (void)hw; (void)us; dmb(); return True; }
static inline Bool SpinLockTimeout(int lock, u32 us) { return SpinLockTimeout_hw(SpinGetHw(lock), us); }
static inline void SpinUnlock(int lock) { (void)lock; dmb(); }
static inline void SpinUnlock_hw(spin_lock_t* hw) { (void)hw; dmb(); }
static inline Bool SpinIsLocked(int lock) { (void)lock; return False; }
static inline void SpinInit(int lock) { SpinUnlock(lock); }
static inline void SpinInit_hw(spin_lock_t* hw) { SpinUnlock_hw(hw); }
static inline void SpinResetAll() {}
static inline u32 SpinGetAll() { return 0; }

// lock spinlock and IRQ (to share resources with interrupt; lock < 0 if not use spinlock)
#define SPINLOCK_LOCK(lock) IRQ_LOCK; if ((lock) >= 0) SpinLock(lock)

// unlock spinlock and IRQ
#define SPINLOCK_UNLOCK(lock) if ((lock) >= 0) SpinUnlock(lock); IRQ_UNLOCK

// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original SDK

static inline spin_lock_t* spin_lock_instance(uint lock_num) { return SpinGetHw(lock_num); }
static inline uint spin_lock_get_num(spin_lock_t *lock) { return SpinGetInx(lock); }
static inline void spin_lock_unsafe_blocking(spin_lock_t *lock) { SpinLock_hw(lock); }
static inline void spin_unlock_unsafe(spin_lock_t *lock) { SpinUnlock_hw(lock); }
static inline u32 spin_lock_blocking(spin_lock_t *lock) { u32 save = LockIRQ(); SpinLock_hw(lock); return save; }
static inline bool is_spin_locked(spin_lock_t *lock) { return SpinIsLocked(SpinGetInx(lock)); }
static inline void spin_unlock(spin_lock_t *lock, u32 saved_irq) { SpinUnlock_hw(lock); UnlockIRQ(saved_irq); }
static inline spin_lock_t *spin_lock_init(uint lock_num) { spin_lock_t* lock = SpinGetHw(lock_num); SpinUnlock_hw(lock); return lock; }
static inline void spin_locks_reset(void) { SpinResetAll(); }
static inline uint next_striped_spin_lock_num(void) { return SpinNextStriped(); }
static inline void spin_lock_claim(uint lock_num) { SpinClaim(lock_num); }
static inline void spin_lock_claim_mask(u32 lock_num_mask) { SpinClaimMask(lock_num_mask); }
static inline void spin_lock_unclaim(uint lock_num) { SpinUnclaim(lock_num); }
static inline int spin_lock_claim_unused(bool required) { (void)required; return SpinClaimFree(); }
static inline bool spin_lock_is_claimed(uint lock_num) { return SpinIsClaimed(lock_num); }

#define remove_volatile_cast(t, x) ({__mem_fence_acquire(); (t)(x); })

struct lock_core {
	spin_lock_t*	spin_lock;	// spin lock protecting this lock's state
};

typedef struct lock_core lock_core_t;

static inline void lock_init(lock_core_t *core, uint lock_num) { core->spin_lock = SpinGetHw(lock_num); }

#define lock_owner_id_t s8
#define LOCK_INVALID_OWNER_ID ((lock_owner_id_t)-1)
#define lock_get_caller_owner_id() ((lock_owner_id_t)get_core_num())
#define lock_is_owner_id_valid(id) ((id)>=0)
#define lock_internal_spin_unlock_with_wait(lock, save) spin_unlock((lock)->spin_lock, save), __wfe()
#define lock_internal_spin_unlock_with_notify(lock, save) spin_unlock((lock)->spin_lock, save), __sev()
#define lock_internal_spin_unlock_with_best_effort_wait_or_timeout(lock, save, until) ( \
	spin_unlock((lock)->spin_lock, save), 	\
	best_effort_wfe_or_timeout(until) )
#define sync_internal_yield_until_before(until) ((void)0)

// === semaphore
typedef struct __attribute__((packed)) __attribute((aligned)) semaphore
{
	struct lock_core	core;
	s16			permits;
	s16			max_permits;
} semaphore_t;

void sem_init(semaphore_t *sem, s16 initial_permits, s16 max_permits);
static inline int sem_available(semaphore_t *sem) { return *(volatile typeof(sem->permits)*) &sem->permits; }
bool __time_critical_func(sem_release)(semaphore_t *sem);
void __time_critical_func(sem_reset)(semaphore_t *sem, s16 permits);
void __time_critical_func(sem_acquire_blocking)(semaphore_t *sem);
bool __time_critical_func(sem_acquire_block_until)(semaphore_t *sem, absolute_time_t until);
static inline bool sem_acquire_timeout_ms(semaphore_t *sem, u32 timeout_ms)
	{ return sem_acquire_block_until(sem, make_timeout_time_ms(timeout_ms)); }
static inline bool sem_acquire_timeout_us(semaphore_t *sem, u32 timeout_us)
	{ return sem_acquire_block_until(sem, make_timeout_time_us(timeout_us)); }
bool __time_critical_func(sem_try_acquire)(semaphore_t *sem);

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // USE_SPINLOCK

#endif // _HOST_SPINLOCK_H
