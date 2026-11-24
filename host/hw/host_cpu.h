// ****************************************************************************
//
//                     Host replacement of _sdk/inc/sdk_cpu.h
//
// ****************************************************************************
// Declarations, constants and macros are taken from PicoLibSDK
// _sdk/inc/sdk_cpu.h (Copyright (c) 2023 Miroslav Nemecek, freely available
// for any purpose). Bodies with ARM/RISC-V assembler or register access are
// replaced by host equivalents (builtins, calls of host_* functions).
// RP2040 build only (RISC-V/RP2350 parts are dropped).

#ifndef _HOST_CPU_H
#define _HOST_CPU_H

#include "_sdk/sdk_addressmap.h"	// Register address offsets
#include "_sdk/inc/sdk_sio.h"		// SIO registers
#include "_sdk/inc/sdk_bootrom.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- host functions (implemented by the emulator, host/*.c)

// let the emulator process pending work (events, IRQ simulation);
// ignored when called from other than the game thread
void host_pump(void);

// emulated global interrupt lock (nestable, recursive)
void host_irq_lock(void);
void host_irq_unlock(void);

// ---- CPU control

// number of CPU cores
#define CORE_NUM	2

// get index of current processor core (0 or 1) - host runs game on core 0
static inline int CpuID(void) { return 0; }

// bit tables
extern const u8 RevBitsTab[16]; // reverse bit table
extern const u8 OrdBitsTab[16]; // order bit table

// compiler barrier
static inline void cb(void) { __asm volatile ("" ::: "memory"); }

// no operation
static inline void nop(void) { cb(); }
static inline void nop2(void) { cb(); }

// disable/enable global interrupts
static inline void di(void) { host_irq_lock(); }
static inline void ei(void) { host_irq_unlock(); }

// save and disable interrupts (the state is not used on host; lock is nestable)
static inline u32 LockIRQ(void) { host_irq_lock(); return 0; }

// restore interrupts
static inline void UnlockIRQ(u32 state) { (void)state; host_irq_unlock(); }

// IRQ lock (temporary disables interrupt)
#define IRQ_LOCK u32 irq_state = LockIRQ()	// lock on begin of critical section
#define IRQ_RELOCK irq_state = LockIRQ()	// lock again on begin of critical section
#define IRQ_UNLOCK UnlockIRQ(irq_state)		// unlock on end of critical section

// busy wait at least number of sys_clk cycles
static inline void BusyWaitCycles(u32 cycles) { (void)cycles; host_pump(); }

// get current IRQ_* if the CPU is handling an exception (IRQ_INVALID = -16 = no exception, thread mode)
static inline int GetCurrentIRQ(void) { return -16; }

// get current stack pointer
static inline void* GetSp(void) { return __builtin_frame_address(0); }

// send event, wait for event, wait for interrupt
static inline void sev(void) { cb(); }
static inline void wfe(void) { host_pump(); }
static inline void wfi(void) { host_pump(); }

// barriers - full memory fence and a chance for the emulator to run
static inline void isb(void) { __sync_synchronize(); host_pump(); }
static inline void dmb(void) { __sync_synchronize(); host_pump(); }
static inline void dsb(void) { __sync_synchronize(); host_pump(); }

// check if CPU is in non-secure mode
static inline Bool NonSecure(void) { return False; }

// reverse byte order of DWORD
static inline u32 Endian(u32 val) { return __builtin_bswap32(val); }

// swap bytes of WORD
static inline u16 Swap(u16 val) { return (u16)((val << 8) | (val >> 8)); }

// swap bytes in two WORDs
static inline u32 Swap2(u32 val) { return ((val & 0x00ff00ffUL) << 8) | ((val >> 8) & 0x00ff00ffUL); }

// rotate bits in DWORD right by 'num' bits
static inline u32 Ror(u32 val, u8 num) { num &= 0x1f; return (num == 0) ? val : ((val >> num) | (val << (32 - num))); }

// rotate bits in DWORD left by 'num' bits
static inline u32 Rol(u32 val, u8 num) { num &= 0x1f; return (num == 0) ? val : ((val << num) | (val >> (32 - num))); }

// reverse 32 bits
static inline u32 Reverse32(u32 val)
{
	val = ((val >> 1) & 0x55555555UL) | ((val & 0x55555555UL) << 1);
	val = ((val >> 2) & 0x33333333UL) | ((val & 0x33333333UL) << 2);
	val = ((val >> 4) & 0x0f0f0f0fUL) | ((val & 0x0f0f0f0fUL) << 4);
	return __builtin_bswap32(val);
}

static inline u32 __rev(u32 val) { return Reverse32(val); }

// reverse 16 bits
static inline u16 Reverse16(u16 val) { return (u16)(Reverse32(val) >> 16); }

// reverse 8 bits (out-of-line in sdk_cpu.c on RP2040)
static inline u8 Reverse8(u8 val) { return (u8)(Reverse32(val) >> 24); }

// reverse 64 bits
static inline u64 Reverse64(u64 val) { return ((u64)Reverse32((u32)val) << 32) | Reverse32((u32)(val >> 32)); }

static inline u64 __revll(u64 val) { return Reverse64(val); }

// get number of leading zeros of u32 number (ROM function 'clz' is not used)
static inline u32 Clz(u32 val) { return (val == 0) ? 32 : (u32)__builtin_clz(val); }

// get number of leading zeros of u64 number
static inline u32 Clz64(u64 num)
{
	if (num >= 0x100000000ULL)
		return Clz((u32)(num >> 32));
	else
		return Clz((u32)num) + 32;
}

static inline u32 clz64(u64 val) { return Clz64(val); }

// get number of trailing zeros of u32 number (ROM function 'ctz' is not used)
static inline u32 Ctz(u32 val) { return (val == 0) ? 32 : (u32)__builtin_ctz(val); }

// get number of trailing zeros of u64 number
static inline u32 Ctz64(u64 num)
{
	if ((u32)num == 0)
		return Ctz((u32)(num >> 32)) + 32;
	else
		return Ctz((u32)num);
}

static inline u32 ctz64(u64 val) { return Ctz64(val); }

// counts '1' bits (population count)
static inline u32 Popcount(u32 val) { return (u32)__builtin_popcount(val); }

static inline u32 Popcount64(u64 val) { return Popcount((u32)val) + Popcount((u32)(val >> 32)); }

// get bit order of 8-bit value (out-of-line in sdk_cpu.c on RP2040)
static inline u8 Order8(u8 val) { return (u8)(32 - Clz((u32)(u8)val)); }

// get bit order of value (returns position of highest bit + 1: 1..32, 0=no bit)
static inline u32 Order(u32 val) { return 32 - Clz(val); }

// get bit order of 64-bit value
static inline u32 Order64(u64 val)
{
	if (val >= 0x100000000ULL)
		return Order((u32)(val >> 32)) + 32;
	else
		return Order((u32)val);
}

// get mask of value (0x123 returns 0x1FF)
static inline u32 Mask(u32 val) { return (val == 0) ? 0 : (((u32)-1) >> Clz(val)); }

// range mask - returns bits set to '1' on range 'first' to 'last'
static inline u32 RangeMask(int first, int last) { return (~(((u32)-1) << (last+1))) & (((u32)-1) << first); }

static inline u64 RangeMask64(int first, int last) { return (~(((u64)-1) << (last+1))) & (((u64)-1) << first); }

// check if integer number is power of 2
static inline Bool IsPow2(u32 a) { return ((a & (a-1)) == 0); }

#if USE_STACKCHECK	// use Stack check (sdk_cpu.c, sdk_cpu.h)

// stack top and bottom of core 0 and core 1 (provided by the emulator)
extern u8 __Stack0Top;
extern u8 __Stack1Top;
extern u8 __Stack0Bottom;
extern u8 __Stack1Bottom;

static inline void* StackTop(void) { return (void*)&__Stack0Top; }
static inline void* StackBottom(void) { return (void*)&__Stack0Bottom; }
static inline int StackFree(void) { return (int)((u8*)GetSp() - (u8*)StackBottom()); }

u32 StackFeed(void);
u32 StackCheck(void);

#endif // USE_STACKCHECK

// fatal error - display panic message and halt execution
void __attribute__((noreturn)) panic(const char *fmt, ...);

void __attribute__((noreturn)) FatalErrorStop();
static inline void __attribute__((noreturn)) __breakpoint() { FatalErrorStop(); }

// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original SDK

static inline void __sev(void) { sev(); }
static inline void __wfe(void) { wfe(); }
static inline void __wfi(void) { wfi(); }
static inline void __dmb(void) { dmb(); }
static inline void __dsb(void) { dsb(); }
static inline void __isb(void) { isb(); }
static inline void __mem_fence_acquire(void) { dmb(); }
static inline void __mem_fence_release(void) { dmb(); }
static inline u32 save_and_disable_interrupts(void) { return LockIRQ(); }
static inline void restore_interrupts(u32 status) { UnlockIRQ(status); }
static inline void tight_loop_contents(void) {}
static inline uint get_core_num(void) { return CpuID(); }

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // _HOST_CPU_H
