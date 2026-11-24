// ****************************************************************************
//
//                    Host replacement of _sdk/inc/sdk_pwm.h
//
// ****************************************************************************
// Generated from PicoLibSDK _sdk/inc/sdk_pwm.h (Copyright (c) 2023 Miroslav
// Nemecek, freely available for any purpose): declarations, constants, types
// and macros are copied verbatim for RP2040 (RP2350/RISC-V branches dropped),
// INLINE -> static inline. Inline bodies that access hardware registers are
// replaced by no-ops returning 0 (marked "HOST: no hardware") or by calls of
// host_* functions implemented by the emulator (marked "HOST").
// Out-of-line functions keep their original prototypes unless marked.

#ifndef _HOST_PWM_H
#define _HOST_PWM_H

#if USE_PWM

#include "_sdk/sdk_addressmap.h"
#include "_sdk/sdk_dreq.h"
#include "host_gpio.h"
#include "host_irq.h"
#if USE_ORIGSDK
#include "_sdk/inc/orig_rp2040/orig_pwm.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---- host functions (implemented by the emulator, host/*.c)

// set compare value of PWM slice channel (audio sample output)
void host_pwm_comp(int slice, int chan, u32 val);

// enable (on != 0) or disable PWM slice
void host_pwm_enable(int slice, int on);


#define PWM_NUM			8		// number of PWM slices
#define PWM_IRQ_NUM		1		// number of IRQs

#define NUM_PWM_SLICES		PWM_NUM

// PWM hardware registers (pwm = 0 to 7)
#define PWM(pwm)	(PWM_BASE + (pwm)*20)	// PWM base (pwm = 0 to 7)

#define PWM_CSR(pwm)	((volatile u32*)(PWM(pwm)+0)) // control and status register
#define PWM_DIV(pwm)	((volatile u32*)(PWM(pwm)+4)) // divider
#define PWM_CTR(pwm)	((volatile u32*)(PWM(pwm)+8)) // counter
#define PWM_CC(pwm)	((volatile u32*)(PWM(pwm)+12)) // counter compare
#define PWM_TOP(pwm)	((volatile u32*)(PWM(pwm)+16)) // counter wrap

#define PWM_EN		((volatile u32*)(PWM_BASE+0xa0)) // enables
#define PWM_INTR	((volatile u32*)(PWM_BASE+0xa4)) // raw interrups
#define PWM_INTE	((volatile u32*)(PWM_BASE+0xa8)) // interrupt enable
#define PWM_INTF	((volatile u32*)(PWM_BASE+0xac)) // interrupt force
#define PWM_INTS	((volatile u32*)(PWM_BASE+0xb0)) // interrupt status

// PWM slice hardware interface
typedef struct pwm_slice_hw {
	io32	csr;	// 0x00: Control and status register
	io32	div;	// 0x04: INT and FRAC form a fixed-point fractional number
	io32	ctr;	// 0x08: Direct access to the PWM counter
	io32	cc;	// 0x0C: Counter compare values
	io32	top;	// 0x10: Counter wrap value
} pwm_slice_hw_t;

STATIC_ASSERT(sizeof(pwm_slice_hw_t) == 0x14, "Incorrect pwm_slice_hw_t!");

// PWM interupt interface
typedef struct {
	io32	inte;	// 0x00: Interrupt Enable
	io32	intf;	// 0x04: Interrupt Force
	io32	ints;	// 0x08: Interrupt status after masking & forcing
} pwm_irq_ctrl_hw_t;

STATIC_ASSERT(sizeof(pwm_irq_ctrl_hw_t) == 0x0C, "Incorrect pwm_irq_ctrl_hw_t!");

// PWM hardware interface
typedef struct {
	pwm_slice_hw_t slice[PWM_NUM]; // 0x00: (8*20=160=0xA0, 12*20=240=0xF0) slices
	io32	en;	// 0xA0 (0xF0): This register aliases the CSR_EN bits for all channels
	io32	intr;	// 0xA4 (0xF4): Raw Interrupts
	union {
		struct {
			io32	inte;	// 0xA8 (0xF8): Interrupt Enable IRQ 0
			io32	intf;	// 0xAC (0xFC): Interrupt Force IRQ 0
			io32	ints;	// 0xB0 (0x100): Interrupt status after masking & forcing IRQ 0
		};
		pwm_irq_ctrl_hw_t irq_ctrl[PWM_IRQ_NUM]; // 0xA8 (0xF8)
	};
} pwm_hw_t;

#define pwm_hw ((pwm_hw_t*)PWM_BASE)

STATIC_ASSERT(sizeof(pwm_hw_t) == 0xB4, "Incorrect pwm_hw_t!");

// PWM slice configuration
typedef struct {
	u32	csr;
	u32	div;
	u32	top;	// counter wrap
} pwm_config;

// clkdiv mode
#define PWM_DIV_FREE	0	// free-running at rate of divider
#define PWM_DIV_HIGH	1	// divider gated by PWM B pin
#define PWM_DIV_RISE	2	// divider advances with rising edge of PWM B pin
#define PWM_DIV_FALL	3	// divider advances with falling edge of PWM B pin

// output channels
#define PWM_CHAN_A	0	// output channel A
#define PWM_CHAN_B	1	// output channel B

// reset PWM slice to default values
static inline void PWM_Reset(int pwm) { host_pwm_enable(pwm, 0); } // HOST

// convert GPIO pin to PWM output channels PWM_CHAN_* (returns 0=A or 1=B)
static inline u8 PWM_GpioToChan(int gpio) { return (u8)(gpio & 1); }
#define PWM_GPIOTOCHAN(gpio) ((gpio) & 1)

// convert GPIO pin to PWM slice (returns 0 to 7 or 0 to 11)
static inline u8 PWM_GpioToSlice(int gpio) { return (u8)((gpio >> 1) & 7); }
#define PWM_GPIOTOSLICE(gpio) (((gpio) >> 1) & 7)

// get PWM slice hardware interface from PWM slice index
static inline pwm_slice_hw_t* PWM_GetHw(int pwm) { return &pwm_hw->slice[pwm]; } // address only

// get PWM slice index from PWM slice hardware interface
static inline u8 PWM_GetInx(const pwm_slice_hw_t* hw) { return (u8)(hw - pwm_hw->slice); } // address only

// get DREQ
static inline u8 PWM_GetDreq(int pwm) { return (u8)(DREQ_PWM_WRAP0 + pwm); }

// === PWM configuration structure (pwm_config)

// get default configuration structure
static inline pwm_config PWM_CfgDef(void)
{
	pwm_config c = {
		0,	// csr: disable, no phase-correct, no invert, free running
		0x010,	// div: divider 1.00
		0xffff	// top: counter wrap = 0xffff
	};
	return c;
}

// apply PWM configuration
static inline void PWM_Cfg_hw(pwm_slice_hw_t* hw, const pwm_config* cfg, Bool start) { (void)cfg; host_pwm_enable(PWM_GetInx(hw), start); } // HOST
static inline void PWM_Cfg(int pwm, const pwm_config* cfg, Bool start) { PWM_Cfg_hw(PWM_GetHw(pwm), cfg, start); }

// config enable phase-correct modulation
static inline void PWM_CfgPhaseCorrect(pwm_config* cfg) { cfg->csr |= B1; }

// config enable trailing-edge modulation (disable phase-correct modulation)
static inline void PWM_CfgTrailingEdge(pwm_config* cfg) { cfg->csr &= ~B1; }

// config inverting output channel enable (chan is output channel PWM_CHAN_*)
static inline void PWM_CfgInvEnable(pwm_config* cfg, int chan) { cfg->csr |= B2 << chan; }

// config inverting output channel disable (chan is output channel PWM_CHAN_*)
static inline void PWM_CfgInvDisable(pwm_config* cfg, int chan) { cfg->csr &= ~(B2 << chan); }

// config set divider mode PWM_DIV_* (default free-running)
static inline void PWM_CfgDivMode(pwm_config* cfg, int divmode) { cfg->csr = (cfg->csr & ~(B4+B5)) | ((u32)divmode << 4); }

// config set clock divider for free-running mode
//  clkdiv ... clock divider * 16, value 0..0xfff (lower 4 bits are fractional part, upper 8 bits are integer part)
static inline void PWM_CfgClkDiv(pwm_config* cfg, int clkdiv) { cfg->div = clkdiv; }
static inline void PWM_CfgClkDivFloat(pwm_config* cfg, float clkdiv) { cfg->div = (u32)(clkdiv*16 + 0.5f); }

// config set counter top wrap value (counter period = top + 1)
static inline void PWM_CfgTop(pwm_config* cfg, u16 top) { cfg->top = top; }

// === PWM setup

// initialize GPIO to use as PWM pin
static inline void PWM_GpioInit(int gpio) { GPIO_Fnc(gpio, GPIO_FNC_PWM); }

// enable PWM slice
static inline void PWM_Enable(int pwm) { host_pwm_enable(pwm, 1); } // HOST
static inline void PWM_Enable_hw(pwm_slice_hw_t* hw) { host_pwm_enable(PWM_GetInx(hw), 1); } // HOST

// enable PWM slices by mask
static inline void PWM_EnableMask(int mask) { int i; for (i = 0; i < PWM_NUM; i++) if ((mask >> i) & 1) host_pwm_enable(i, 1); } // HOST

// disable PWM slice
static inline void PWM_Disable(int pwm) { host_pwm_enable(pwm, 0); } // HOST
static inline void PWM_Disable_hw(pwm_slice_hw_t* hw) { host_pwm_enable(PWM_GetInx(hw), 0); } // HOST

// disable PWM slices by mask
static inline void PWM_DisableMask(int mask) { int i; for (i = 0; i < PWM_NUM; i++) if ((mask >> i) & 1) host_pwm_enable(i, 0); } // HOST

// enable phase-correct modulation
static inline void PWM_PhaseCorrect(int pwm) { } // HOST: no hardware
static inline void PWM_PhaseCorrect_hw(pwm_slice_hw_t* hw) { } // HOST: no hardware

// enable trailing-edge modulation (disable phase-correct modulation)
static inline void PWM_TrailingEdge(int pwm) { } // HOST: no hardware
static inline void PWM_TrailingEdge_hw(pwm_slice_hw_t* hw) { } // HOST: no hardware

// inverting output channel enable (chan is output channel PWM_CHAN_*)
static inline void PWM_InvEnable(int pwm, int chan) { } // HOST: no hardware
static inline void PWM_InvEnable_hw(pwm_slice_hw_t* hw, int chan) { } // HOST: no hardware

// inverting output channel disable (chan is output channel PWM_CHAN_*)
static inline void PWM_InvDisable(int pwm, int chan) { } // HOST: no hardware
static inline void PWM_InvDisable_hw(pwm_slice_hw_t* hw, int chan) { } // HOST: no hardware

// set divider mode PWM_DIV_* (default free-running)
static inline void PWM_DivMode(int pwm, int divmode) { } // HOST: no hardware
static inline void PWM_DivMode_hw(pwm_slice_hw_t* hw, int divmode) { } // HOST: no hardware

// retard phase of counter by 1 (must be running)
static inline void PWM_Retard(int pwm) { } // HOST: no hardware
static inline void PWM_Retard_hw(pwm_slice_hw_t* hw) { } // HOST: no hardware

// retard phase of counter by 1 and wait to complete (must be running)
static inline void PWM_RetardWait(int pwm) { } // HOST: no hardware
static inline void PWM_RetardWait_hw(pwm_slice_hw_t* hw) { } // HOST: no hardware

// advance phase of counter by 1 (must be running)
static inline void PWM_Advance(int pwm) { } // HOST: no hardware
static inline void PWM_Advance_hw(pwm_slice_hw_t* hw) { } // HOST: no hardware

// advance phase of counter by 1 and wait to complete (must be running)
static inline void PWM_AdvanceWait(int pwm) { } // HOST: no hardware
static inline void PWM_AdvanceWait_hw(pwm_slice_hw_t* hw) { } // HOST: no hardware

// set clock divider for free-running mode
//  clkdiv ... clock divider * 16, value 0..0xfff (lower 4 bits are fractional part, upper 8 bits are integer part)
static inline void PWM_ClkDiv(int pwm, int clkdiv) { } // HOST: no hardware
static inline void PWM_ClkDiv_hw(pwm_slice_hw_t* hw, int clkdiv) { } // HOST: no hardware

static inline void PWM_ClkDivFloat(int pwm, float clkdiv) { } // HOST: no hardware
static inline void PWM_ClkDivFloat_hw(pwm_slice_hw_t* hw, float clkdiv) { } // HOST: no hardware

// get clock divider (clock divider * 16, 12-bit value, lower 4 bits are fractional part, upper 8 bits are integer part)
static inline u16 PWM_GetClkDiv(int pwm) { return (u16)0; } // HOST: no hardware
static inline u16 PWM_GetClkDiv_hw(pwm_slice_hw_t* hw) { return (u16)0; } // HOST: no hardware

// set clock counter (16-bit value)
static inline void PWM_Count(int pwm, u16 cnt) { } // HOST: no hardware
static inline void PWM_Count_hw(pwm_slice_hw_t* hw, u16 cnt) { } // HOST: no hardware

// get clock counter
static inline u16 PWM_GetCount(int pwm) { return (u16)0; } // HOST: no hardware
static inline u16 PWM_GetCount_hw(pwm_slice_hw_t* hw) { return (u16)0; } // HOST: no hardware

// set compare value of output channel PWM_CHAN_* (cmp is 16-bit value)
static inline void PWM_Comp(int pwm, int chan, u16 cmp) { host_pwm_comp(pwm, chan, cmp); } // HOST
static inline void PWM_Comp_hw(pwm_slice_hw_t* hw, int chan, u16 cmp) { host_pwm_comp(PWM_GetInx(hw), chan, cmp); } // HOST

// set compare value of both output channels (cmp is 16-bit value)
static inline void PWM_Comp2(int pwm, u16 cmp_a, u16 cmp_b) { host_pwm_comp(pwm, 0, cmp_a); host_pwm_comp(pwm, 1, cmp_b); } // HOST
static inline void PWM_Comp2_hw(pwm_slice_hw_t* hw, u16 cmp_a, u16 cmp_b) { PWM_Comp2(PWM_GetInx(hw), cmp_a, cmp_b); } // HOST

// get compare value of output channel PWM_CHAN_*
static inline u16 PWM_GetComp(int pwm, int chan) { return (u16)0; } // HOST: no hardware
static inline u16 PWM_GetComp_hw(const pwm_slice_hw_t* hw, int chan) { return (u16)0; } // HOST: no hardware

// set counter top wrap value (counter period = top + 1)
static inline void PWM_Top(int pwm, u16 top) { } // HOST: no hardware
static inline void PWM_Top_hw(pwm_slice_hw_t* hw, u16 top) { } // HOST: no hardware

// get counter top wrap value
static inline u16 PWM_GetTop(int pwm) { return (u16)0; } // HOST: no hardware
static inline u16 PWM_GetTop_hw(const pwm_slice_hw_t* hw) { return (u16)0; } // HOST: no hardware

// set clock frequency in Hz for free-running mode
//  freq ... frequency, for clk_sys = 125 MHz in range 488 kHz to 125 MHz
//     (for TOP=256 sampling rate 1.9 kHz to 488 kHz)
static inline void PWM_Clock(int pwm, u32 freq) { } // HOST: no hardware
static inline void PWM_Clock_hw(pwm_slice_hw_t* hw, u32 freq) { } // HOST: no hardware

// get real frequency of the PWM clock (in Hz)
u32 PWM_GetClock(int pwm);
u32 PWM_GetClock_hw(pwm_slice_hw_t* hw);

// Find system clock in Hz that sets the most accurate PWM clock frequency.
u32 PWM_FindSysClk(u32 min_hz, u32 max_hz, u32 freq);

// === PWM interrupt

// get raw interrupt flag (not masked by EN)
static inline Bool PWM_IntRaw(int pwm) { return (Bool)0; } // HOST: no hardware

// clear interrupt flag
static inline void PWM_IntClear(int pwm) { } // HOST: no hardware

// enable interrupt, IRQ_PWM_WRAP or IRQ_PWM_WRAP_0
static inline void PWM_IntEnable(int pwm) { } // HOST: no hardware

// disable interrupt, IRQ_PWM_WRAP or IRQ_PWM_WRAP_0
static inline void PWM_IntDisable(int pwm) { } // HOST: no hardware

// force interrupt, IRQ_PWM_WRAP or IRQ_PWM_WRAP_0
static inline void PWM_IntForce(int pwm) { } // HOST: no hardware

// unforce interrupt, IRQ_PWM_WRAP or IRQ_PWM_WRAP_0
static inline void PWM_IntUnforce(int pwm) { } // HOST: no hardware

// check if interrupt is forced, IRQ_PWM_WRAP or IRQ_PWM_WRAP_0
static inline Bool PWM_IntIsForced(int pwm) { return (Bool)0; } // HOST: no hardware

// get interrupt status (masked by EN), IRQ_PWM_WRAP or IRQ_PWM_WRAP_0
static inline Bool PWM_IntState(int pwm) { return (Bool)0; } // HOST: no hardware


// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original SDK

// PWM Divider mode settings
enum pwm_clkdiv_mode
{
	PWM_DIV_FREE_RUNNING = 0,	// Free-running counting at rate dictated by fractional divider
	PWM_DIV_B_HIGH = 1,		// Fractional divider is gated by the PWM B pin
	PWM_DIV_B_RISING = 2,		// Fractional divider advances with each rising edge of the PWM B pin
	PWM_DIV_B_FALLING = 3		// Fractional divider advances with each falling edge of the PWM B pin
};

// PWM output channel
#undef PWM_CHAN_A
#undef PWM_CHAN_B
enum pwm_chan
{
	PWM_CHAN_A = 0,
	PWM_CHAN_B = 1
};

// Determine the PWM slice that is attached to the specified GPIO
static inline uint pwm_gpio_to_slice_num(uint gpio) { return PWM_GpioToSlice(gpio); }

// Determine the PWM channel that is attached to the specified GPIO.
static inline uint pwm_gpio_to_channel(uint gpio) { return PWM_GpioToChan(gpio); }

// Set phase correction in a PWM configuration
static inline void pwm_config_set_phase_correct(pwm_config *c, bool phase_correct)
	{ if (phase_correct) PWM_CfgPhaseCorrect(c); else PWM_CfgTrailingEdge(c); }

// Set PWM clock divider in a PWM configuration
static inline void pwm_config_set_clkdiv(pwm_config *c, float div) { PWM_CfgClkDivFloat(c, div); }

// Set PWM clock divider in a PWM configuration using an 8:4 fractional value
static inline void pwm_config_set_clkdiv_int_frac(pwm_config *c, uint integer, u8 fract)
	{ PWM_CfgClkDiv(c, integer*16 + fract); }

// Set PWM clock divider in a PWM configuration
static inline void pwm_config_set_clkdiv_int(pwm_config *c, uint div) { PWM_CfgClkDiv(c, div*16); }

// Set PWM counting mode in a PWM configuration
static inline void pwm_config_set_clkdiv_mode(pwm_config *c, enum pwm_clkdiv_mode mode) { PWM_CfgDivMode(c, mode); }

// Set output polarity in a PWM configuration
static inline void pwm_config_set_output_polarity(pwm_config *c, bool a, bool b)
{
	if (a) PWM_CfgInvEnable(c, PWM_CHAN_A); else PWM_CfgInvDisable(c, PWM_CHAN_A);
	if (b) PWM_CfgInvEnable(c, PWM_CHAN_B); else PWM_CfgInvDisable(c, PWM_CHAN_B);
}

// PWM counter wrap value in a PWM configuration
static inline void pwm_config_set_wrap(pwm_config *c, u16 wrap) { PWM_CfgTop(c, wrap); }

// Initialise a PWM with settings from a configuration object
static inline void pwm_init(uint slice_num, pwm_config *c, bool start) { PWM_Cfg(slice_num, c, start); }

// Get a set of default values for PWM configuration
static inline pwm_config pwm_get_default_config(void) { return PWM_CfgDef(); }

// Set the current PWM counter wrap value
static inline void pwm_set_wrap(uint slice_num, u16 wrap) { PWM_Top(slice_num, wrap); }

// Set the current PWM counter compare value for one channel
static inline void pwm_set_chan_level(uint slice_num, uint chan, u16 level) { PWM_Comp(slice_num, chan, level); }

// Set PWM counter compare values
static inline void pwm_set_both_levels(uint slice_num, u16 level_a, u16 level_b) { PWM_Comp2(slice_num, level_a, level_b); } // HOST

// Helper function to set the PWM level for the slice and channel associated with a GPIO.
static inline void pwm_set_gpio_level(uint gpio, u16 level)
	{ PWM_Comp(PWM_GpioToSlice(gpio), PWM_GpioToChan(gpio), level); }

// Get PWM counter
static inline u16 pwm_get_counter(uint slice_num) { return PWM_GetCount(slice_num); }

// Set PWM counter
static inline void pwm_set_counter(uint slice_num, u16 c) { PWM_Count(slice_num, c); }

// Advance PWM count
static inline void pwm_advance_count(uint slice_num) { PWM_AdvanceWait(slice_num); }

// Retard PWM count
static inline void pwm_retard_count(uint slice_num) { PWM_RetardWait(slice_num); }

// Set PWM clock divider using an 8:4 fractional value
static inline void pwm_set_clkdiv_int_frac(uint slice_num, u8 integer, u8 fract)
	{ PWM_ClkDiv(slice_num, integer*16 + fract); }

// Set PWM clock divider
static inline void pwm_set_clkdiv(uint slice_num, float divider) { PWM_ClkDivFloat(slice_num, divider); }

// Set PWM output polarity
static inline void pwm_set_output_polarity(uint slice_num, bool a, bool b)
{
	if (a) PWM_InvEnable(slice_num, PWM_CHAN_A); else PWM_InvDisable(slice_num, PWM_CHAN_A);
	if (b) PWM_InvEnable(slice_num, PWM_CHAN_B); else PWM_InvDisable(slice_num, PWM_CHAN_B);
}

// Set PWM divider mode
static inline void pwm_set_clkdiv_mode(uint slice_num, enum pwm_clkdiv_mode mode) { PWM_DivMode(slice_num, mode); }

// Set PWM phase correct on/off
static inline void pwm_set_phase_correct(uint slice_num, bool phase_correct)
	{ if (phase_correct) PWM_PhaseCorrect(slice_num); else PWM_TrailingEdge(slice_num); }

// Enable/Disable PWM
static inline void pwm_set_enabled(uint slice_num, bool enabled)
	{ if (enabled) PWM_Enable(slice_num); else PWM_Disable(slice_num); }

// Enable/Disable multiple PWM slices simultaneously
static inline void pwm_set_mask_enabled(u32 mask) { int i; for (i = 0; i < PWM_NUM; i++) host_pwm_enable(i, (int)((mask >> i) & 1)); } // HOST

// Enable PWM instance interrupt
static inline void pwm_set_irq_enabled(uint slice_num, bool enabled)
	{ if (enabled) PWM_IntEnable(slice_num); else PWM_IntDisable(slice_num); }

// Enable multiple PWM instance interrupts
static inline void pwm_set_irq_mask_enabled(uint32_t slice_mask, bool enabled) { } // HOST: no hardware

// Clear a single PWM channel interrupt
static inline void pwm_clear_irq(uint slice_num) { PWM_IntClear(slice_num); }

// Get PWM interrupt status, raw
static inline u32 pwm_get_irq_status_mask(void) { return (u32)0; } // HOST: no hardware

// Force PWM interrupt
static inline void pwm_force_irq(uint slice_num) { PWM_IntForce(slice_num); }

// Return the DREQ to use for pacing transfers to a particular PWM slice
static inline uint pwm_get_dreq(uint slice_num) { return PWM_GetDreq(slice_num); }

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // USE_PWM

#endif // _HOST_PWM_H
