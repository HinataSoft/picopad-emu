// ****************************************************************************
//
//                     Host replacement of _sdk/inc/sdk_irq.h
//
// ****************************************************************************
// Declarations, constants and macros are taken from PicoLibSDK
// _sdk/inc/sdk_irq.h (Copyright (c) 2023 Miroslav Nemecek, freely available
// for any purpose). NVIC/SCB register access is replaced by no-ops,
// SetHandler() registers the handler with the emulator. RP2040 only.

#ifndef _HOST_IRQ_H
#define _HOST_IRQ_H

#if USE_IRQ	// use IRQ interrupts (sdk_irq.c, sdk_irq.h)

#include "_sdk/sdk_addressmap.h"	// Register address offsets
#include "host_cpu.h"
#include "_sdk/inc/sdk_scb.h"

#if USE_ORIGSDK		// include interface of original SDK
#include "_sdk/inc/orig_rp2040/orig_m0plus.h"	// constants of original SDK
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "_sdk/inc/sdk_irq_rp2040.h"	// IRQ_* numbers

#define VTABLE_FIRST_IRQ	16	// ARM

#define IRQ_PRIO_REALTIME	0	// highest priority - real-time time-critical interrupts
#define IRQ_PRIO_SYSTICK	1	// high priority - SysTick
#define IRQ_PRIO_NORMAL		2	// default normal priority
#define IRQ_PRIO_PENDSV		3	// lowest priority - PendSV handler

typedef void (*irq_handler_t)(void);

// register IRQ handler with the emulator (host function, host/*.c)
void host_set_handler(int irq, void (*fn)(void));

// NVIC register addresses (kept for compatibility, never accessed on host)
#define NVIC_ISER ((volatile u32*)(PPB_BASE + 0xE100))	// interrupt set-enable register
#define NVIC_ICER ((volatile u32*)(PPB_BASE + 0xE180))	// interrupt clear-enable register
#define NVIC_ISPR ((volatile u32*)(PPB_BASE + 0xE200))	// interrupt set-pending register
#define NVIC_ICPR ((volatile u32*)(PPB_BASE + 0xE280))	// interrupt clear-pending register
#define NVIC_IPR0 ((volatile u32*)(PPB_BASE + 0xE400))	// interrupt priority registers

typedef u32 iqr_mask_t;		// 32-bit IRQ mask
static inline u32 IrqRangeMask(int first, int last) { return RangeMask(first, last); }

typedef struct {
	io32	iser;		// 0x000: Interrupt Set-Enable Register
	io32	_pad0[31];
	io32	icer;		// 0x080: Interrupt Clear-Enable Register
	io32	_pad1[31];
	io32	ispr;		// 0x100: Interrupt Set-Pending Register
	io32	_pad2[31];
	io32	icpr;		// 0x180: Interrupt Clear-Pending Register
	io32	_pad3[95];
	io32	ipr[8];		// 0x300: Interrupt Priority Register
} nvic_hw_t;

#define nvic_hw ((nvic_hw_t *)(PPB_BASE + 0xE100))

#define SCB_CPUID	((volatile u32*)(PPB_BASE + 0xED00))	// CPUID Base Register
#define SCB_ICSR	((volatile u32*)(PPB_BASE + 0xED04))	// Interrupt Control and State Register
#define SCB_VTOR	((volatile u32*)(PPB_BASE + 0xED08))	// Vector Table Offset Register
#define SCB_AIRCR	((volatile u32*)(PPB_BASE + 0xED0C))	// Application Interrupt and Reset Control Register
#define SCB_SCR		((volatile u32*)(PPB_BASE + 0xED10))	// System Control Register
#define SCB_SHPR2	((volatile u32*)(PPB_BASE + 0xED1C))	// system handler priority 2
#define SCB_SHPR3	((volatile u32*)(PPB_BASE + 0xED20))	// system handler priority 3

#define VECTOR_TABLE_SIZE	((IRQ_NUM+VTABLE_FIRST_IRQ)*4)

// thumb bit helpers (pure address arithmetic)
static inline void* SetThumbBit(void* addr) { return addr; }
static inline void* ClrThumbBit(void* addr) { return addr; }

// set/get interrupt service handler
static inline void SetHandler(int irq, irq_handler_t handler) { host_set_handler(irq, handler); }
static inline irq_handler_t GetHandler(int irq) { (void)irq; return (irq_handler_t)0; } // not tracked on host

// NVIC control - no-ops on host (the emulator decides when handlers run)
static inline void NVIC_IRQClearMask(iqr_mask_t mask) { (void)mask; }
static inline void NVIC_IRQClear(int irq) { (void)irq; }
static inline void NVIC_IRQForceMask(iqr_mask_t mask) { (void)mask; }
static inline void NVIC_IRQForce(int irq) { (void)irq; }
static inline Bool NVIC_IRQIsPending(int irq) { (void)irq; return False; }
static inline void NVIC_IRQEnableMask(iqr_mask_t mask) { (void)mask; }
static inline void NVIC_IRQEnable(int irq) { (void)irq; }
static inline void NVIC_IRQDisableMask(iqr_mask_t mask) { (void)mask; }
static inline void NVIC_IRQDisable(int irq) { (void)irq; }
static inline Bool NVIC_IRQIsEnabled(int irq) { (void)irq; return False; }
static inline void NVIC_IRQPrio(int irq, u8 prio) { (void)irq; (void)prio; }
static inline void NVIC_IRQPrioDef(void) {}
static inline void NVIC_SVCallPrio(int prio) { (void)prio; }
static inline void NVIC_PendSVPrio(int prio) { (void)prio; }
static inline void NVIC_PendSVForce(void) {}
static inline Bool NVIC_PendSVIsPending(void) { return False; }
static inline void NVIC_PendSVClear(void) {}
static inline void NVIC_SysTickPrio(int prio) { (void)prio; }
static inline void NVIC_SysTickForce(void) {}
static inline Bool NVIC_SysTickIsPending(void) { return False; }
static inline void NVIC_SystickClear(void) {}
static inline void NVIC_NMIForce(void) {}

// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original SDK

static inline void irq_set_priority(uint num, u8 hardware_priority) { NVIC_IRQPrio(num, hardware_priority); }
static inline uint irq_get_priority(uint num) { (void)num; return IRQ_PRIO_NORMAL; }
static inline void irq_set_enabled(uint num, bool enabled) { if (enabled) NVIC_IRQEnable(num); else NVIC_IRQDisable(num); }
static inline bool irq_is_enabled(uint num) { return NVIC_IRQIsEnabled(num); }
static inline void irq_set_mask_enabled(uint32_t mask, bool enabled) { if (enabled) NVIC_IRQEnableMask(mask); else NVIC_IRQDisableMask(mask); }
static inline void irq_set_exclusive_handler(int num, irq_handler_t handler) { SetHandler(num, handler); }
static inline irq_handler_t irq_get_exclusive_handler(int num) { return GetHandler(num); }
static inline void irq_add_shared_handler(uint num, irq_handler_t handler, u8 order_priority) { (void)order_priority; SetHandler(num, handler); }
static inline void irq_remove_handler(uint num, irq_handler_t handler) { (void)num; (void)handler; }
static inline bool irq_has_shared_handler(uint num) { (void)num; return false; }
static inline irq_handler_t irq_get_vtable_handler(int num) { return GetHandler(num); }
static inline void irq_clear(uint int_num) { NVIC_IRQClear(int_num); }
static inline void irq_set_pending(uint num) { NVIC_IRQForce(num); }
static inline void irq_init_priorities(void) { NVIC_IRQPrioDef(); }

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // USE_IRQ

#endif // _HOST_IRQ_H
