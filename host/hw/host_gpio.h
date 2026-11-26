// ****************************************************************************
//
//                    Host replacement of _sdk/inc/sdk_gpio.h
//
// ****************************************************************************
// Generated from PicoLibSDK _sdk/inc/sdk_gpio.h (Copyright (c) 2023 Miroslav
// Nemecek, freely available for any purpose): declarations, constants, types
// and macros are copied verbatim for RP2040 (RP2350/RISC-V branches dropped),
// INLINE -> static inline. Inline bodies that access hardware registers are
// replaced by no-ops returning 0 (marked "HOST: no hardware") or by calls of
// host_* functions implemented by the emulator (marked "HOST").
// Out-of-line functions keep their original prototypes unless marked.

#ifndef _HOST_GPIO_H
#define _HOST_GPIO_H

#include "_sdk/sdk_addressmap.h"
#include "host_cpu.h"
#include "host_irq.h"
#include "_sdk/inc/sdk_sio.h"
#include "_sdk/inc/sdk_gpio_coproc.h"
#if USE_ORIGSDK
#include "_sdk/inc/orig_rp2040/orig_io_bank0.h"
#endif
#if USE_ORIGSDK
#include "_sdk/inc/orig_rp2040/orig_pads_bank0.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---- host functions (implemented by the emulator, host/*.c)

// get input level (0 or 1) of GPIO pin (buttons are active low)
u8 host_gpio_in(int pin);

// sound output pin PWMSND_GPIO driven as a GPIO (bit-bang sound, host/audio.c):
// set / get its level 0 or 1; the other output pins are not modelled
void host_gpio_snd_level(int level);
int host_gpio_snd_get(void);
#ifdef PWMSND_GPIO
#define HOST_GPIO_SND_PIN	PWMSND_GPIO
#else
#define HOST_GPIO_SND_PIN	(-1)
#endif
#define HOST_GPIO_SND_BIT	((HOST_GPIO_SND_PIN >= 0) ? (1u << HOST_GPIO_SND_PIN) : 0u)


// number of pins

#define GPIO_PIN_NUM		30	// number of GPIO pin registers
#define GPIO_INT_REGNUM		4	// number of GPIO interrupt registers
#define GPIO_PIN_SWCLK		30	// SWCLK pin (only pad functions)
#define GPIO_PIN_SWD		31	// SWD pin (only pad functions)
typedef u32 gpio_mask_t;		// 32-bit pin mask
static inline u32 PinRangeMask(int first, int last) { return RangeMask(first, last); } // mask of range of pins


// === GPIO pad control (pin = 0..31/RP2040 or 0..49/RP2350, including SWCLK and SWD)

// pad control hardware registers
#define GPIO_VOLT	((volatile u32*)(PADS_BANK0_BASE+0))	// voltage select
#define GPIO_PAD(pin) ((volatile u32*)(PADS_BANK0_BASE+(pin)*4+4)) // GPIO pad control register, pin = 0..31 or 0..49 (including SWCLK and SWD)

// pad control hardware interface
typedef struct {
	io32	voltage_select;		// 0x00: voltage select
	io32	io[GPIO_PIN_NUM];	// 0x04: pad control register
	io32	swclk;			// 0x7C: SWCLK pin pad control
	io32	swd;			// 0x80: SWD pin pad control
} pads_bank0_hw_t;

STATIC_ASSERT(sizeof(pads_bank0_hw_t) == 0x84, "Incorrect pads_bank0_hw_t!");

#define pads_bank0_hw ((pads_bank0_hw_t*)PADS_BANK0_BASE)
#define padsbank0_hw_t pads_bank0_hw_t
#define padsbank0_hw pads_bank0_hw

// === GPIO pin control (pin = 0..29/RP2040 or 0..47/RP2350)

// pin control hardware registers
#define GPIO_STATUS(pin) ((volatile u32*)(IO_BANK0_BASE+(pin)*8)) // GPIO status (read only), pin = 0..29 or 0..47
#define GPIO_CTRL(pin) ((volatile u32*)(IO_BANK0_BASE+(pin)*8+4)) // GPIO pin control, pin = 0..29 or 0..47

// pin control hardware interface
typedef struct {
	io32	status;		// 0x00: GPIO status
	io32	ctrl;		// 0x04: GPIO control including function select and overrides
} io_bank0_status_ctrl_hw_t;

STATIC_ASSERT(sizeof(io_bank0_status_ctrl_hw_t) == 0x08, "Incorrect io_bank0_status_ctrl_hw_t!");

#define iobank0_status_ctrl_hw_t io_bank0_status_ctrl_hw_t

// === GPIO interrupt control (pin = 0..29/RP2040 or 0..47/RP2350)

// pin interrupt hardware registers
// IRQ control registers, array of 4*u32 (cpu = processor core 0 or 1; or 2 = dormant wake)
//  Interrupts EDGE are latched in INTR registers and must be cleared.
//  Interrupts LEVEL are not latched, they become inactive on end condition.
#define GPIO_IRQ_INTR ((volatile u32*)(IO_BANK0_BASE+0xf0)) // raw interrupt of both CPUs
#define GPIO_IRQ_INTE(cpu) ((volatile u32*)(IO_BANK0_BASE+(cpu)*0x30+0x100)) // interrupt enable
#define GPIO_IRQ_INTF(cpu) ((volatile u32*)(IO_BANK0_BASE+(cpu)*0x30+0x100+0x10)) // force interrupt
#define GPIO_IRQ_INTS(cpu) ((volatile u32*)(IO_BANK0_BASE+(cpu)*0x30+0x100+0x20)) // interrupt status

// pin interrupt hardware interface
typedef struct {
	io32	inte[GPIO_INT_REGNUM];	// 0x00: Interrupt Enable
	io32	intf[GPIO_INT_REGNUM];	// 0x10 (0x18): Interrupt Force
	io32	ints[GPIO_INT_REGNUM];	// 0x20 (0x30): Interrupt status after masking & forcing
} io_bank0_irq_ctrl_hw_t;

STATIC_ASSERT(sizeof(io_bank0_irq_ctrl_hw_t) == 0x30, "Incorrect io_bank0_irq_ctrl_hw_t!");

// pin bank interface
typedef struct {
	io_bank0_status_ctrl_hw_t	io[GPIO_PIN_NUM];	// 0x000: pin control
	io32				intr[GPIO_INT_REGNUM];		// 0x0F0 (0x230): Raw Interrupts
	union {
		struct {
			io_bank0_irq_ctrl_hw_t	proc0_irq_ctrl;		// 0x100 (0x248): interrupt control for proc0
			io_bank0_irq_ctrl_hw_t	proc1_irq_ctrl;		// 0x130 (0x290): interrupt control for proc1
			io_bank0_irq_ctrl_hw_t	dormant_wake_irq_ctrl;	// 0x160 (0x2D8): dormant interrupt control
		};
		io_bank0_irq_ctrl_hw_t	irq_ctrl[3];			// 0x100 (0x248):
	};
} io_bank0_hw_t;

STATIC_ASSERT(sizeof(io_bank0_hw_t) == 0x190, "Incorrect io_bank0_hw_t!");

#define io_bank0_hw ((io_bank0_hw_t*)IO_BANK0_BASE)
#define iobank0_hw io_bank0_hw

// === GPIO pin data (pin = 0..29/RP2040 or 0..47,56..63/RP2350 including USB and QSPI)

// GPIO pin data registers (bit 0..29 = pin 0..29)
//  (SIO does not support aliases for atomic access!)

#define GPIO_IN_DATA		((volatile u32*)(SIO_BASE+4))		// GPIO input
#define GPIO_OUT_DATA		((volatile u32*)(SIO_BASE+0x10))	// GPIO output (read: last value written to output)
#define GPIO_OUT_SET		((volatile u32*)(SIO_BASE+0x14))	// GPIO output set
#define GPIO_OUT_CLR		((volatile u32*)(SIO_BASE+0x18))	// GPIO output clear
#define GPIO_OUT_XOR		((volatile u32*)(SIO_BASE+0x1C))	// GPIO output XOR
#define GPIO_OE			((volatile u32*)(SIO_BASE+0x20))	// GPIO output enable (read: last value written)
#define GPIO_OE_SET		((volatile u32*)(SIO_BASE+0x24))	// GPIO output enable set
#define GPIO_OE_CLR		((volatile u32*)(SIO_BASE+0x28))	// GPIO output enable clear
#define GPIO_OE_XOR		((volatile u32*)(SIO_BASE+0x2C))	// GPIO output enable XOR

// GPIO pin functions
#define GPIO_FNC_XIP	0	// external Flash
#define GPIO_FNC_SPI	1	// SPI (SPI0 or SPI1)
#define GPIO_FNC_UART	2	// UART (UART0 or UART1)
#define GPIO_FNC_I2C	3	// I2C (I2C0 or I2C1)
#define GPIO_FNC_PWM	4	// PWM (PWM0..PWM7)
#define GPIO_FNC_SIO	5	// SIO (GPIO digital function)
#define GPIO_FNC_PIO0	6	// PIO0
#define GPIO_FNC_PIO1	7	// PIO1
#define GPIO_FNC_GPCK	8	// clock (only pins 20..25)
#define GPIO_FNC_USB	9	// USB
#define GPIO_FNC_NULL	31	// no function (use in case of ADC input)

// interrupt events
#define IRQ_EVENT_LEVELLOW	B0	// level low (not latched interrupt request)
#define IRQ_EVENT_LEVELHIGH	B1	// level high (not latched interrupt request)
#define IRQ_EVENT_EDGELOW	B2	// edge low (request is latched in NTR register and must be cleared)
#define IRQ_EVENT_EDGEHIGH	B3	// edge high (request is latched in NTR register and must be cleared)

#define IRQ_EVENT_ALL		0x0f	// all events
#define IRQ_EVENT_NONE		0	// no event

// drive strength
#define GPIO_DRIVE_2MA		0
#define GPIO_DRIVE_4MA		1
#define GPIO_DRIVE_8MA		2
#define GPIO_DRIVE_12MA		3

// peripheral signal override
#define GPIO_OVER_NORMAL	0
#define GPIO_OVER_INVERT	1
#define GPIO_OVER_LOW		2
#define GPIO_OVER_HIGH		3
#define GPIO_OVER_MASK		3

// generic callback function type for GPIO events (install with GPIO_IRQSetCallback() function)
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events
typedef void (*gpio_irq_callback_t)(uint pin, u32 events);

// generic callback for GPIO types (NULL = not used)
extern gpio_irq_callback_t GPIO_Callbacks[CORE_NUM];

// ==== GPIO pad control (pin = 0..31/RP2040 or 0..49/RP2350, including SWCLK and SWD, or hw = pad control register from GPIO_PadHw())

// GPIO set voltage to 3.3V (DVDD >= 2V5; default state)
static inline void GPIO_Voltage3V3() { } // HOST: no hardware

// GPIO set voltage to 1.8V (DVDD <= 1V8)
static inline void GPIO_Voltage1V8() { } // HOST: no hardware

// get pointer to pad control interface (pin = 0..31 or 0..49)
static inline io32* GPIO_PadHw(int pin) { return &pads_bank0_hw->io[pin]; } // address only

// init pad to default reset state (pin = 0..29 or 0..47; partially valid for SWCLK and SWD)
//  - slow slew rate, enable schmitt, pull-down, 4mA, in/out enable (RP2350: input disable, enable isolation state)
static inline void GPIO_PadInit(int pin) { } // HOST: no hardware
static inline void GPIO_PadInit_hw(io32* hw) { } // HOST: no hardware


// GPIO output enable; disable has priority over GPIO_DirOut (default state; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_OutEnable(int pin) { } // HOST: no hardware
static inline void GPIO_OutEnable_hw(io32* hw) { } // HOST: no hardware

// GPIO output disable; disable has priority over GPIO_DirOut (pin = 0..31 or 0..49, including SWCLK and SWD; default state for SWCLK pin)
static inline void GPIO_OutDisable(int pin) { } // HOST: no hardware
static inline void GPIO_OutDisable_hw(io32* hw) { } // HOST: no hardware

// enable pin input (default state on RP2040; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_InEnable(int pin) { } // HOST: no hardware
static inline void GPIO_InEnable_hw(io32* hw) { } // HOST: no hardware

// disable pin input (should be used on ADC input; default state on RP2350; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_InDisable(int pin) { } // HOST: no hardware
static inline void GPIO_InDisable_hw(io32* hw) { } // HOST: no hardware

// set output strength to 2 mA (pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Drive2mA(int pin) { } // HOST: no hardware
static inline void GPIO_Drive2mA_hw(io32* hw) { } // HOST: no hardware

// set output strength to 4 mA (default state; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Drive4mA(int pin) { } // HOST: no hardware
static inline void GPIO_Drive4mA_hw(io32* hw) { } // HOST: no hardware

// set output strength to 8 mA (pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Drive8mA(int pin) { } // HOST: no hardware
static inline void GPIO_Drive8mA_hw(io32* hw) { } // HOST: no hardware

// set output strength to 12 mA (pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Drive12mA(int pin) { } // HOST: no hardware
static inline void GPIO_Drive12mA_hw(io32* hw) { } // HOST: no hardware

// Set drive strength GPIO_DRIVE_* (pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Drive(int pin, int drive) { } // HOST: no hardware
static inline void GPIO_Drive_hw(io32* hw, int drive) { } // HOST: no hardware

// set no pulls (pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_NoPull(int pin) { } // HOST: no hardware
static inline void GPIO_NoPull_hw(io32* hw) { } // HOST: no hardware

// set pull down (default state; pin = 0..31 or 0..49, including SWCLK and SWD)
// RP2350-A2 errata RP2350-E9:
//  Description:
//   If pad is set as input and voltage is within undefine range between VIL and VIH,
//   then pull-down will not pull voltage low, because leakage current will hold at 2V.
//  Workaround 1:
//   Use external pull-down 8.2 kOhm.
//  Workaroung 2:
//   Disabling input will reset the leakage. To read pulled-down input state, enable
//   input immediately before reading GPIO_InEnable(), and then re-disable GPIO_InDisable().
static inline void GPIO_PullDown(int pin) { } // HOST: no hardware
static inline void GPIO_PullDown_hw(io32* hw) { } // HOST: no hardware

// set pull up (pin = 0..31 or 0..49, including SWCLK and SWD; default state for SWCLK and SWD pins)
static inline void GPIO_PullUp(int pin) { } // HOST: no hardware
static inline void GPIO_PullUp_hw(io32* hw) { } // HOST: no hardware

// set bus keep (weak pull up and down; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_BusKeep(int pin) { } // HOST: no hardware
static inline void GPIO_BusKeep_hw(io32* hw) { } // HOST: no hardware

// enable schmitt trigger (uses hysteresis on input; default state; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_SchmittEnable(int pin) { } // HOST: no hardware
static inline void GPIO_SchmittEnable_hw(io32* hw) { } // HOST: no hardware

// disable schmitt trigger (will not use hysteresis on input; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_SchmittDisable(int pin) { } // HOST: no hardware
static inline void GPIO_SchmittDisable_hw(io32* hw) { } // HOST: no hardware

// use slow slew rate control on output (default state; pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Slow(int pin) { } // HOST: no hardware
static inline void GPIO_Slow_hw(io32* hw) { } // HOST: no hardware

// use fast slew rate control on output (pin = 0..31 or 0..49, including SWCLK and SWD)
static inline void GPIO_Fast(int pin) { } // HOST: no hardware
static inline void GPIO_Fast_hw(io32* hw) { } // HOST: no hardware

// ==== GPIO pin control (pin = 0..29/RP2040 or 0..47/RP2350)

// get pointer to pin control interface (pin = 0..29 or 0..47)
static inline io_bank0_status_ctrl_hw_t* GPIO_PinHw(int pin) { return &io_bank0_hw->io[pin]; } // address only

// get output (0 or 1) from peripheral, before override (pin = 0..29 or 0..47)
static inline u8 GPIO_OutPeri(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_OutPeri_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get output (0 or 1) to pad, after override (pin = 0..29 or 0..47)
static inline u8 GPIO_OutPad(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_OutPad_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get output enable (0 or 1) from peripheral, before override (pin = 0..29 or 0..47)
static inline u8 GPIO_OePeri(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_OePeri_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get output enable (0 or 1) to pad, after override (pin = 0..29 or 0..47)
static inline u8 GPIO_OePad(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_OePad_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get input (0 or 1) from pad, before override (pin = 0..29 or 0..47)
static inline u8 GPIO_InPad(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_InPad_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get raw input (0 or 1) from pad, before override and before muxing, enables pin input (pin = 0..29 or 0..47)
static inline u8 GPIO_RawIn(int pin)
{
	GPIO_InEnable(pin);
	return GPIO_InPad(pin);
}

// get input (0 or 1) to peripheral, after override (pin = 0..29 or 0..47)
static inline u8 GPIO_InPeri(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_InPeri_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get interrupt (0 or 1) from pad, before override (pin = 0..29 or 0..47)
static inline u8 GPIO_IrqPad(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_IrqPad_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// get interrupt (0 or 1) to processor, after override (pin = 0..29 or 0..47)
static inline u8 GPIO_IrqProc(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_IrqProc_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

#define GPIO_OUTOVER_LSB	8

// set Output pin override - normal (default state; pin = 0..29 or 0..47)
static inline void GPIO_OutOverNormal(int pin) { } // HOST: no hardware
static inline void GPIO_OutOverNormal_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Output pin override - invert (pin = 0..29 or 0..47)
static inline void GPIO_OutOverInvert(int pin) { } // HOST: no hardware
static inline void GPIO_OutOverInvert_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Output pin override - low (pin = 0..29 or 0..47)
static inline void GPIO_OutOverLow(int pin) { } // HOST: no hardware
static inline void GPIO_OutOverLow_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Output pin override - high (pin = 0..29 or 0..47)
static inline void GPIO_OutOverHigh(int pin) { } // HOST: no hardware
static inline void GPIO_OutOverHigh_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

#define GPIO_OEOVER_LSB		12

// set Output enable pin override - normal (default state; pin = 0..29 or 0..47)
static inline void GPIO_OEOverNormal(int pin) { } // HOST: no hardware
static inline void GPIO_OEOverNormal_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Output enable pin override - invert (pin = 0..29 or 0..47)
static inline void GPIO_OEOverInvert(int pin) { } // HOST: no hardware
static inline void GPIO_OEOverInvert_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Output enable pin override - disable (pin = 0..29 or 0..47)
static inline void GPIO_OEOverDisable(int pin) { } // HOST: no hardware
static inline void GPIO_OEOverDisable_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Output enable pin override - enable (pin = 0..29 or 0..47)
static inline void GPIO_OEOverEnable(int pin) { } // HOST: no hardware
static inline void GPIO_OEOverEnable_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Input pin override - normal (default state; pin = 0..29 or 0..47)
static inline void GPIO_InOverNormal(int pin) { } // HOST: no hardware
static inline void GPIO_InOverNormal_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Input pin override - invert (pin = 0..29 or 0..47)
static inline void GPIO_InOverInvert(int pin) { } // HOST: no hardware
static inline void GPIO_InOverInvert_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Input pin override - low (pin = 0..29 or 0..47)
static inline void GPIO_InOverLow(int pin) { } // HOST: no hardware
static inline void GPIO_InOverLow_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set Input pin override - high (pin = 0..29 or 0..47)
static inline void GPIO_InOverHigh(int pin) { } // HOST: no hardware
static inline void GPIO_InOverHigh_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set IRQ pin override - normal (default state; pin = 0..29 or 0..47)
static inline void GPIO_IRQOverNormal(int pin) { } // HOST: no hardware
static inline void GPIO_IRQOverNormal_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set IRQ pin override - invert (pin = 0..29 or 0..47)
static inline void GPIO_IRQOverInvert(int pin) { } // HOST: no hardware
static inline void GPIO_IRQOverInvert_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set IRQ pin override - low (pin = 0..29 or 0..47)
static inline void GPIO_IRQOverLow(int pin) { } // HOST: no hardware
static inline void GPIO_IRQOverLow_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set IRQ pin override - high (pin = 0..29 or 0..47)
static inline void GPIO_IRQOverHigh(int pin) { } // HOST: no hardware
static inline void GPIO_IRQOverHigh_hw(io_bank0_status_ctrl_hw_t* hw) { } // HOST: no hardware

// set GPIO function GPIO_FNC_* (pin = 0..29 or 0..47)
// Resets pad overrides, enables input and output.
// To setup GPIO:
//  - reset GPIO to default state with GPIO_Reset()
//  - setup output level, output enable, pull-up/pull-down, schmitt, slew, drive strength
//  - set GPIO function
//  - if needed, disable input (ADC) and set overrides
static inline void GPIO_Fnc(int pin, int fnc) { } // HOST: no hardware

// set GPIO function GPIO_FNC_* with mask (bit '1' to set function of this pin)
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_FncMask(gpio_mask_t mask, int fnc) { } // HOST: no hardware

// set GPIO function GPIO_FNC_* raw, resets pad overrides, no other side effects (pin = 0..29 or 0..47)
static inline void GPIO_FncRaw(int pin, int fnc) { } // HOST: no hardware

// get current GPIO function GPIO_FNC_* (pin = 0..29 or 0..47)
static inline u8 GPIO_GetFnc(int pin) { return (u8)0; } // HOST: no hardware
static inline u8 GPIO_GetFnc_hw(const io_bank0_status_ctrl_hw_t* hw) { return (u8)0; } // HOST: no hardware

// === GPIO pin interrupt control (pin = 0..29/RP2040 or 0..47/RP2350)

// acknowledge IRQ interrupt for both CPU
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_EDGE* of events to acknowledge
//  Interrupts LEVEL are not latched, they become inactive on end condition.
static inline void GPIO_IRQAck(int pin, int events) { } // HOST: no hardware

// enable IRQ interrupt for selected/current CPU
//   cpu = CPU core 0 or 1, use CpuID() to get current core, or 2 = dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to enable

// After enabling interrupt from pin, following steps are necessary:
//   - set handler of IRQ_IO_BANK0 (use function GPIO_SetHandler), or use isr_io_bank0 handler
//   - not necessary, but rather clear pending interrupt of IRQ_IO_BANK0 (use function NVIC_IRQClear)
//   - enable NVIC interrupt of IRQ_IO_BANK0 (use function NVIC_IRQEnable)
//   - enable global interrupt ei()

static inline void GPIO_IRQEnableCpu(int cpu, int pin, int events) { } // HOST: no hardware
static inline void GPIO_IRQEnable(int pin, int events) { GPIO_IRQEnableCpu(CpuID(), pin, events); }

// disable IRQ interrupt for selected/current CPU or dormant
//   cpu = CPU core 0 or 1, use CpuID() to get current core, or 2 = dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to disable
static inline void GPIO_IRQDisableCpu(int cpu, int pin, int events) { } // HOST: no hardware
static inline void GPIO_IRQDisable(int pin, int events) { GPIO_IRQDisableCpu(CpuID(), pin, events); }

// set IRQ handler
static inline void GPIO_SetHandler(irq_handler_t handler) { SetHandler(IRQ_IO_BANK0, handler); }

// force IRQ interrupt for selected/current CPU
//   cpu = CPU core 0 or 1, use CpuID() to get current core, or 2 = dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to force
static inline void GPIO_IRQForceCpu(int cpu, int pin, int events) { } // HOST: no hardware
static inline void GPIO_IRQForce(int pin, int events) { GPIO_IRQForceCpu(CpuID(), pin, events); }

// clear force IRQ interrupt for selected/current CPU
//   cpu = CPU core 0 or 1, use CpuID() to get current core, or 2 = dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to unforce
static inline void GPIO_IRQUnforceCpu(int cpu, int pin, int events) { } // HOST: no hardware
static inline void GPIO_IRQUnforce(int pin, int events) { GPIO_IRQUnforceCpu(CpuID(), pin, events); }

// check IRQ interrupt status for selected/current CPU (returns 1 if IRQ is pending)
//   cpu = CPU core 0 or 1, use CpuID() to get current core, or 2 = dormant wake
//   pin = 0..29 or 0..47
//   returns events = bit mask with IRQ_EVENT_* of incoming events
static inline u8 GPIO_IRQIsPendingCpu(int cpu, int pin) { return 0; } // HOST: no hardware
static inline u8 GPIO_IRQIsPending(int pin) { return GPIO_IRQIsPendingCpu(CpuID(), pin); }

// check if IRQ is forced for selected/current CPU (returns 1 if IRQ is pending)
//   cpu = CPU core 0 or 1, use CpuID() to get current core, or 2 = dormant wake
//   pin = 0..29 or 0..47
//   returns events = bit mask with IRQ_EVENT_* of forced events
static inline u8 GPIO_IRQIsForcedCpu(int cpu, int pin) { return 0; } // HOST: no hardware
static inline u8 GPIO_IRQIsForced(int pin) { return GPIO_IRQIsForcedCpu(CpuID(), pin); }

// enable IRQ interrupt for dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to enable
static inline void GPIO_IRQEnableDorm(int pin, int events) { } // HOST: no hardware

// disable IRQ interrupt for dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to disable
static inline void GPIO_IRQDisableDorm(int pin, int events) { } // HOST: no hardware

// force IRQ interrupt for dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to force
static inline void GPIO_IRQForceDorm(int pin, int events) { } // HOST: no hardware

// clear force IRQ interrupt for dormant wake
//   pin = 0..29 or 0..47
//   events = bit mask with IRQ_EVENT_* of events to unforce
static inline void GPIO_IRQUnforceDorm(int pin, int events) { } // HOST: no hardware

// check IRQ interrupt status for dormant wake (returns 1 if IRQ is pending)
//   pin = 0..29 or 0..47
//   returns events = bit mask with IRQ_EVENT_* of incoming events
static inline u8 GPIO_IRQIsPendingDorm(int pin) { return 0; } // HOST: no hardware

// check if IRQ is forced for dormant wake (returns 1 if IRQ is pending)
//   pin = 0..29 or 0..47
//   returns events = bit mask with IRQ_EVENT_* of forced events
static inline u8 GPIO_IRQIsForcedDorm(int pin) { return 0; } // HOST: no hardware

// set generic callback for current core (NULL=disable callback)
// - it will install default IRQ handler (as used with GPIO_SetHandler)
// To use:
// - set callback using the GPIO_IRQSetCallback() function
// - enable interrupts on GPIO pins using the GPIO_IRQEnable() function
// - enable NVIC interrupt using the NVIC_IRQEnable(IRQ_IO_BANK0) command
static inline void GPIO_IRQSetCallback(gpio_irq_callback_t cb) { } // HOST: no hardware

// === GPIO pin data (pin = 0..29/RP2040 or 0..47,56..63/RP2350 including USB and QSPI)

// get input pin (returns 0 or 1; pin = 0..29 or 0..47,56..63)
static inline u8 GPIO_In(int pin) { return host_gpio_in(pin); } // HOST

// get all input pins
static inline gpio_mask_t GPIO_InAll() { gpio_mask_t m = 0; int i; for (i = 0; i < GPIO_PIN_NUM; i++) if (host_gpio_in(i)) m |= BIT(i); return m; } // HOST

// output 0 to pin (pin = 0..29 or 0..47,56..63)
static inline void GPIO_Out0(int pin) { if (pin == HOST_GPIO_SND_PIN) host_gpio_snd_level(0); } // HOST

// output 1 to pin (pin = 0..29 or 0..47,56..63)
static inline void GPIO_Out1(int pin) { if (pin == HOST_GPIO_SND_PIN) host_gpio_snd_level(1); } // HOST

// flip output to pin (pin = 0..29 or 0..47,56..63)
static inline void GPIO_Flip(int pin) { if (pin == HOST_GPIO_SND_PIN) host_gpio_snd_level(!host_gpio_snd_get()); } // HOST

// output value to pin (val = 0 or val != 0; pin = 0..29 or 0..47,56..63)
static inline void GPIO_Out(int pin, int val)
{
	if (val == 0)
		GPIO_Out0(pin);
	else
		GPIO_Out1(pin);
}

// output all pins
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_OutAll(gpio_mask_t value) { if (HOST_GPIO_SND_BIT) host_gpio_snd_level((value & HOST_GPIO_SND_BIT) != 0); } // HOST

// output masked values to pins
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_OutMask(gpio_mask_t mask, gpio_mask_t value) { if (mask & HOST_GPIO_SND_BIT) host_gpio_snd_level((value & HOST_GPIO_SND_BIT) != 0); } // HOST

// clear output pins masked
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_ClrMask(gpio_mask_t mask) { if (mask & HOST_GPIO_SND_BIT) host_gpio_snd_level(0); } // HOST

// set output pins masked
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_SetMask(gpio_mask_t mask) { if (mask & HOST_GPIO_SND_BIT) host_gpio_snd_level(1); } // HOST

// flip output pins masked
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_FlipMask(gpio_mask_t mask) { if (mask & HOST_GPIO_SND_BIT) host_gpio_snd_level(!host_gpio_snd_get()); } // HOST

// get last output value to pin (returns 0 or 1; pin = 0..29 or 0..47,56..63)
static inline u8 GPIO_GetOut(int pin) { return (u8)((pin == HOST_GPIO_SND_PIN) ? host_gpio_snd_get() : 0); } // HOST

// get all last output values to pins
static inline gpio_mask_t GPIO_GetOutAll() { return host_gpio_snd_get() ? (gpio_mask_t)HOST_GPIO_SND_BIT : (gpio_mask_t)0; } // HOST

// set output direction of pin (enable output mode; pin = 0..29 or 0..47,56..63)
static inline void GPIO_DirOut(int pin) { } // HOST: no hardware

// set input direction of pin (disable output mode; pin = 0..29 or 0..47,56..63)
static inline void GPIO_DirIn(int pin) { } // HOST: no hardware

// set pin output direction 0 in, <>0 out (pin = 0..29 or 0..47,56..63)
static inline void GPIO_Dir(int pin, int val)
{
	if (val == 0)
		GPIO_DirIn(pin);
	else
		GPIO_DirOut(pin);
}

// set output direction of pins masked
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_DirOutMask(gpio_mask_t mask) { } // HOST: no hardware

// set input direction of pins masked
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_DirInMask(gpio_mask_t mask) { } // HOST: no hardware

// set direction masked
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_SetDirMask(gpio_mask_t mask, gpio_mask_t outval) { } // HOST: no hardware

// set direction of all pins
static inline void GPIO_SetDirAll(gpio_mask_t outval) { } // HOST: no hardware

// get output direction of pin (returns 0=input or 1=output; pin = 0..29 or 0..47,56..63)
static inline u8 GPIO_GetDir(int pin) { return (u8)0; } // HOST: no hardware

// get output direction of all pins (0=input, 1=output)
static inline gpio_mask_t GPIO_GetDirAll() { return (gpio_mask_t)0; } // HOST: no hardware

// initialize GPIO pin base function, reset pin, set input mode, LOW output value (pin = 0..29 or 0..47,56..63)
static inline void GPIO_Init(int pin) { } // HOST: no hardware

// initialize GPIO pin base function masked (bit '1' = initialize this pin)
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_InitMask(gpio_mask_t mask) { } // HOST: no hardware

// reset GPIO pin (return to reset state) (pin = 0..29 or 0..47,56..63)
static inline void GPIO_Reset(int pin) { } // HOST: no hardware

// reset GPIO pins masked (return to reset state)
// To use pin mask in range (first..last), use function PinRangeMask.
static inline void GPIO_ResetMask(gpio_mask_t mask) { } // HOST: no hardware

// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original SDK

#define NUM_BANK0_GPIOS	30		// number of GPIO pins
typedef u32 raw_irq_mask_type_t;	// 32-bit pin mask

// functions
enum gpio_function {
	GPIO_FUNC_XIP = 0,
	GPIO_FUNC_SPI = 1,
	GPIO_FUNC_UART = 2,
	GPIO_FUNC_I2C = 3,
	GPIO_FUNC_PWM = 4,
	GPIO_FUNC_SIO = 5,
	GPIO_FUNC_PIO0 = 6,
	GPIO_FUNC_PIO1 = 7,
	GPIO_FUNC_GPCK = 8,
	GPIO_FUNC_USB = 9,
	GPIO_FUNC_NULL = 0x1f,
};

// direction flag
#define GPIO_OUT 1
#define GPIO_IN 0

// GPIO Interrupt level definitions (GPIO events)
//  Level High: the GPIO pin is a logical 1 (not latched interrupt request)
//  Level Low: the GPIO pin is a logical 0 (not latched interrupt request)
//  Edge High: the GPIO has transitioned from a logical 0 to a logical 1 (interrupt request is latched in INTR register)
//  Edge Low: the GPIO has transitioned from a logical 1 to a logical 0 (interrupt request is latched in INTR register)
enum gpio_irq_level {
	GPIO_IRQ_LEVEL_LOW = 0x1u,
	GPIO_IRQ_LEVEL_HIGH = 0x2u,
	GPIO_IRQ_EDGE_FALL = 0x4u,
	GPIO_IRQ_EDGE_RISE = 0x8u,
};

// Slew rate limiting levels for GPIO outputs
enum gpio_slew_rate {
	GPIO_SLEW_RATE_SLOW = 0,	// Slew rate limiting enabled
	GPIO_SLEW_RATE_FAST = 1		// Slew rate limiting disabled
};

// Drive strength levels for GPIO outputs
enum gpio_drive_strength {
	GPIO_DRIVE_STRENGTH_2MA = 0,
	GPIO_DRIVE_STRENGTH_4MA = 1,
	GPIO_DRIVE_STRENGTH_8MA = 2,
	GPIO_DRIVE_STRENGTH_12MA = 3
};

enum gpio_override {
	GPIO_OVERRIDE_NORMAL	= 0,
	GPIO_OVERRIDE_INVERT	= 1,
	GPIO_OVERRIDE_LOW	= 2,
	GPIO_OVERRIDE_HIGH	= 3,
};

// === Pad Controls + IO Muxing

// Select GPIO function
static inline void gpio_set_function(uint gpio, enum gpio_function fn) { GPIO_Fnc(gpio, fn); }

// Determine current GPIO function
static inline enum gpio_function gpio_get_function(uint gpio) { return (enum gpio_function)GPIO_GetFnc(gpio); }

// Select up and down pulls on specific GPIO
static inline void gpio_set_pulls(uint gpio, bool up, bool down) { } // HOST: no hardware

// Set specified GPIO to be pulled up.
static inline void gpio_pull_up(uint gpio) { GPIO_PullUp(gpio); }

// Determine if the specified GPIO is pulled up.
static inline bool gpio_is_pulled_up(uint gpio) { return (bool)0; } // HOST: no hardware

// Set specified GPIO to be pulled down.
static inline void gpio_pull_down(uint gpio) { GPIO_PullDown(gpio); }

// Determine if the specified GPIO is pulled down.
static inline bool gpio_is_pulled_down(uint gpio) { return (bool)0; } // HOST: no hardware

// Disable pulls on specified GPIO
static inline void gpio_disable_pulls(uint gpio) { GPIO_NoPull(gpio); }

// Set GPIO IRQ override
static inline void gpio_set_irqover(uint gpio, enum gpio_override over) { } // HOST: no hardware

// Set GPIO output override
static inline void gpio_set_outover(uint gpio, enum gpio_override over) { } // HOST: no hardware

// Select GPIO input override
static inline void gpio_set_inover(uint gpio, enum gpio_override over) { } // HOST: no hardware

// Select GPIO output enable override
static inline void gpio_set_oeover(uint gpio, enum gpio_override over) { } // HOST: no hardware

// Enable GPIO input
static inline void gpio_set_input_enabled(uint gpio, bool enabled)
	{ if (enabled) GPIO_InEnable(gpio); else GPIO_InDisable(gpio); }

// Enable/disable GPIO input hysteresis (Schmitt trigger)
static inline void gpio_set_input_hysteresis_enabled(uint gpio, bool enabled)
	{ if (enabled)  GPIO_SchmittEnable(gpio); else GPIO_SchmittDisable(gpio); }

// Determine whether input hysteresis is enabled on a specified GPIO
static inline bool gpio_is_input_hysteresis_enabled(uint gpio) { return (bool)0; } // HOST: no hardware

// Set output slew rate for a specified GPIO
static inline void gpio_set_slew_rate(uint gpio, enum gpio_slew_rate slew)
	{ if (slew == GPIO_SLEW_RATE_SLOW) GPIO_Slow(gpio); else GPIO_Fast(gpio); }

// Determine current slew rate for a specified GPIO
static inline enum gpio_slew_rate gpio_get_slew_rate(uint gpio) { return (enum gpio_slew_rate)0; } // HOST: no hardware

// Set drive strength for a specified GPIO
static inline void gpio_set_drive_strength(uint gpio, enum gpio_drive_strength drive) { GPIO_Drive(gpio, drive); }

// Determine current drive strength for a specified GPIO
static inline enum gpio_drive_strength gpio_get_drive_strength(uint gpio) { return (enum gpio_drive_strength)0; } // HOST: no hardware

// Enable or disable specific interrupt events for specified GPIO
// Events is a bitmask of the following \ref gpio_irq_level values:
// bit | constant            | interrupt
// ----|----------------------------------------------------------
//   0 | GPIO_IRQ_LEVEL_LOW  | Continuously while level is low
//   1 | GPIO_IRQ_LEVEL_HIGH | Continuously while level is high
//   2 | GPIO_IRQ_EDGE_FALL  | On each transition from high to low
//   3 | GPIO_IRQ_EDGE_RISE  | On each transition from low to high
static inline void gpio_set_irq_enabled(uint gpio, uint32_t event_mask, bool enabled)
	{ if (enabled) GPIO_IRQEnable(gpio, event_mask); else GPIO_IRQDisable(gpio, event_mask); }

// Set the generic callback used for GPIO IRQ events for the current core
static inline void gpio_set_irq_callback(gpio_irq_callback_t callback) { GPIO_IRQSetCallback(callback); }

// Convenience function which performs multiple GPIO IRQ related initializations
static inline void gpio_set_irq_enabled_with_callback(uint gpio, u32 event_mask, bool enabled, gpio_irq_callback_t callback)
{
	gpio_set_irq_enabled(gpio, event_mask, enabled);
	gpio_set_irq_callback(callback);
	if (enabled) irq_set_enabled(IO_IRQ_BANK0, true);
}

// Enable dormant wake up interrupt for specified GPIO and events
static inline void gpio_set_dormant_irq_enabled(uint gpio, uint32_t event_mask, bool enabled)
	{ if (enabled) GPIO_IRQEnableDorm(gpio, event_mask); else GPIO_IRQDisableDorm(gpio, event_mask); }

// Return the current interrupt status (pending events) for the given GPIO
static inline uint32_t gpio_get_irq_event_mask(uint gpio) { return GPIO_IRQIsPending(gpio); }

// Acknowledge a GPIO interrupt for the specified events on the calling core
static inline void gpio_acknowledge_irq(uint gpio, uint32_t event_mask) { GPIO_IRQAck(gpio, event_mask); }

// Initialise a GPIO for (enabled I/O and set func to GPIO_FUNC_SIO)
static inline void gpio_init(uint gpio) { GPIO_Init(gpio); }

// Resets a GPIO back to the NULL function, i.e. disables it.
static inline void gpio_deinit(uint gpio) { GPIO_Reset(gpio); }

// Initialise multiple GPIOs (enabled I/O and set func to GPIO_FUNC_SIO)
static inline void gpio_init_mask(uint gpio_mask) { GPIO_InitMask(gpio_mask); }

// === Input

// Get state of a single specified GPIO
static inline bool gpio_get(uint gpio) { return GPIO_In(gpio) != 0; }

// Get raw value of all GPIOs
static inline u32 gpio_get_all(void) { return GPIO_InAll(); }

// === Output

// Drive high every GPIO appearing in mask
static inline void gpio_set_mask(u32 mask) { GPIO_SetMask(mask); }

// Drive low every GPIO appearing in mask
static inline void gpio_clr_mask(u32 mask) { GPIO_ClrMask(mask); }

// Toggle every GPIO appearing in mask
static inline void gpio_xor_mask(u32 mask) { GPIO_FlipMask(mask); }

// Drive GPIO high/low depending on parameters
static inline void gpio_put_masked(u32 mask, u32 value) { GPIO_OutMask(mask, value); }

// Drive all pins simultaneously
static inline void gpio_put_all(u32 value) { GPIO_OutAll(value); }

// Drive a single GPIO high/low
static inline void gpio_put(uint gpio, bool value) { GPIO_Out(gpio, value); }

// Determine whether a GPIO is currently driven high or low
static inline bool gpio_get_out_level(uint gpio) { return GPIO_GetOut(gpio) != 0; }

// === Direction

// Set a number of GPIOs to output
static inline void gpio_set_dir_out_masked(u32 mask) { GPIO_DirOutMask(mask); }

// Set a number of GPIOs to input
static inline void gpio_set_dir_in_masked(u32 mask) { GPIO_DirInMask(mask); }

// Set multiple GPIO directions
static inline void gpio_set_dir_masked(u32 mask, u32 value) { GPIO_SetDirMask(mask, value); }

// Set direction of all pins simultaneously.
static inline void gpio_set_dir_all_bits(u32 values) { GPIO_SetDirAll(values); }

// Set a single GPIO direction
static inline void gpio_set_dir(uint gpio, bool out) { if (out) GPIO_DirOut(gpio); else GPIO_DirIn(gpio); }

// Check if a specific GPIO direction is OUT
static inline bool gpio_is_dir_out(uint gpio) { return GPIO_GetDir(gpio) != 0; }

// Get a specific GPIO direction
static inline uint gpio_get_dir(uint gpio) { return GPIO_GetDir(gpio); }

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // _HOST_GPIO_H
