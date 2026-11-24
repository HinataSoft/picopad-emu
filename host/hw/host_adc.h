// ****************************************************************************
//
//                    Host replacement of _sdk/inc/sdk_adc.h
//
// ****************************************************************************
// Generated from PicoLibSDK _sdk/inc/sdk_adc.h (Copyright (c) 2023 Miroslav
// Nemecek, freely available for any purpose): declarations, constants, types
// and macros are copied verbatim for RP2040 (RP2350/RISC-V branches dropped),
// INLINE -> static inline. Inline bodies that access hardware registers are
// replaced by no-ops returning 0 (marked "HOST: no hardware") or by calls of
// host_* functions implemented by the emulator (marked "HOST").
// Out-of-line functions keep their original prototypes unless marked.

#ifndef _HOST_ADC_H
#define _HOST_ADC_H

#if USE_ADC

#include "_sdk/sdk_addressmap.h"
#if USE_ORIGSDK
#include "_sdk/inc/orig_rp2040/orig_adc.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


// ADC hardware registers
#define ADC_CS		((volatile u32*)(ADC_BASE+0x00)) // control and status
#define ADC_RESULT	((volatile u32*)(ADC_BASE+0x04)) // result
#define ADC_FCS		((volatile u32*)(ADC_BASE+0x08)) // FIFO control and status
#define ADC_FIFO	((volatile u32*)(ADC_BASE+0x0C)) // conversion result FIFO
#define ADC_DIV		((volatile u32*)(ADC_BASE+0x10)) // clock divider
#define ADC_INTR	((volatile u32*)(ADC_BASE+0x14)) // raw interrupt
#define ADC_INTE	((volatile u32*)(ADC_BASE+0x18)) // interrupt enable
#define ADC_INTF	((volatile u32*)(ADC_BASE+0x1C)) // interrupt force
#define ADC_INTS	((volatile u32*)(ADC_BASE+0x20)) // interrupt status

// ADC hardware interface
typedef struct {
	io32	cs;		// 0x00: control and status
	io32	result;		// 0x04: result
	io32	fcs;		// 0x08: FIFO control and status
	io32	fifo;		// 0x0C: FIFO result
	io32	div;		// 0x10: clock divider
	io32	intr;		// 0x14: raw interrupt
	io32	inte;		// 0x18: interrupt enable
	io32	intf;		// 0x1C: interrupt force
	io32	ints;		// 0x20: interrupt status
} adc_hw_t;

#define	adc_hw	((adc_hw_t*)ADC_BASE)

STATIC_ASSERT(sizeof(adc_hw_t) == 0x24, "Incorrect adc_hw_t!");

// ADC inputs
#define ADC_MUX_GPIO26	0	// pin GPIO 26
#define ADC_MUX_GPIO27	1	// pin GPIO 27
#define ADC_MUX_GPIO28	2	// pin GPIO 28
#define ADC_MUX_GPIO29	3	// pin GPIO 29
#define ADC_MUX_TEMP	4	// temperature sensor

// ADC init
void ADC_Init(void);

// ADC terminate
void ADC_Term(void);

// ADC disable
static inline void ADC_Disable() { } // HOST: no hardware

// ADC enable
static inline void ADC_Enable() { } // HOST: no hardware

// initialize GPIO to use as an ADC pin
//  pin ... pin 26 to 29 (or 40 to 47)
void ADC_PinInit(int pin);

// terminate pin as ADC input
//  pin ... pin 26 to 29 (or 40 to 47)
void ADC_PinTerm(int pin);

// select ADC input
//  input ... input 0 to 4 or 0 to 8 (use ADC_MUX_*)
static inline void ADC_Mux(int input) { } // HOST: no hardware

// get currently selected ADC input (returns 0 to 4 or 0 to 8)
static inline u8 ADC_GetMux(void) { return (u8)0; } // HOST: no hardware

// set round robin sampling selector
//  mask ... mask of bits B0 to B4 (or B0 to B8) of inputs (use BIT(ADC_MUX_*))
//           set 0 to disable round robin sampling
static inline void ADC_RoundRobin(int mask) { } // HOST: no hardware

// enable temperature sensor
void ADC_TempEnable(void);

// disable temperature sensor (saves 40 uA)
static inline void ADC_TempDisable(void) { } // HOST: no hardware

// start single conversion
static inline void ADC_StartOnce(void) { } // HOST: no hardware

// check if ADC is ready (False if conversion is in progress)
static inline Bool ADC_Ready(void) { return True; } // HOST: no hardware (always ready, never blocks)

// wait for end of conversion
static inline void ADC_Wait(void) { while (!ADC_Ready()) {} }

// get last conversion result
static inline u16 ADC_Result(void) { return (u16)0; } // HOST: no hardware

// do single conversion (takes 96 clock cycles = min. 2 us on 48 MHz clock, real 2.6 us)
u16 ADC_Single(void);

// do single conversion with denoise (returns value 0..0xffff; takes min. 32 us on 48 MHz clock, real 43 us)
u16 ADC_SingleDenoise(void);

// do single conversion and recalculate to voltage on 3.3V
float ADC_SingleU(void);

// do single conversion and recalculate to voltage on 3.3V, integer in mV (range 0..3300)
int ADC_SingleUint(void);

// enable continuously repeated conversion (free-running sampling mode)
static inline void ADC_MultiEnable(void) { } // HOST: no hardware

// disable continuously repeated conversion
static inline void ADC_MultiDisable(void) { } // HOST: no hardware

// check if most recent ADC conversion encountered an error; result is undefined or noisy
static inline Bool ADC_Err(void) { return (Bool)0; } // HOST: no hardware

// some past ADC conversion encountered an error (clear with ADC_ErrClear())
static inline Bool ADC_ErrSticky(void) { return (Bool)0; } // HOST: no hardware

// clear sticky error
static inline void ADC_ErrClear(void) { } // HOST: no hardware

// set ADC clock divisor
//  clkdiv ... clock divisor * 256 (if non-zero, ADC_MultiEnable() will start conversions with that interval)
// Interval of samples will be (1 + clkdiv/256) cycles. One conversion takes 96 cycles, so minimal value
// of clkdiv is 96*256 = 0x5F00 (period 2 us, 500 kHz). Maximal value is 0xFFFFFF (period 1365 us, 732 Hz).
static inline void ADC_ClkDiv(int clkdiv) { } // HOST: no hardware

// set ADC clock divisor as float (with resolution 1/256)
//  clkdiv ... clock divisor (if non-zero, ADC_MultiEnable() will start conversions with that interval)
// Interval of samples will be (1 + clkdiv) cycles. One conversion takes 96 cycles, so minimal value
// of clkdiv is 96 (period 2 us, 500 kHz). Maximal value is 65535 (period 1365 us, 732 Hz).
void ADC_ClkDivFloat(float clkdiv);

// set ADC sampling frequency of repeated conversions
//  freq ... sampling frequency in Hz, minimal 732 Hz, maximal 500000 Hz, 0 = switch off
void ADC_Freq(u32 freq);

// get current sampling frequency in Hz (732 to 500000 Hz, 0 = off)
u32 ADC_GetFreq(void);

// setup ADC FIFO (FIFO is 4 samples long)
//  en ... True to enable write each conversion result to the FIFO
//  shift ... True to right-shift result to be one byte 8-bit in size (enables DMA to byte buffers)
//  err ... True means bit 15 of FIFO contains error flag for each sample
//  dreq_en ... enable DMA requests when FIFO contains data
//  dreq_thresh ... threshold for DMA requests/FIFO IRQ if enabled (DREQ/IRQ asserted then level >= threshold)
void ADC_FifoSetup(Bool en, Bool shift, Bool err, Bool dreq_en, int dreq_thresh);

// check if ADC FIFO is empty
static inline Bool ADC_IsEmpty(void) { return True; } // HOST: no hardware (FIFO always empty)

// check if ADC FIFO is full
static inline Bool ADC_IsFull(void) { return (Bool)0; } // HOST: no hardware

// check if ADC FIFO has been underflow
static inline Bool ADC_IsUnder(void) { return (Bool)0; } // HOST: no hardware

// check if ADC FIFO has been overflow
static inline Bool ADC_IsOver(void) { return (Bool)0; } // HOST: no hardware

// clear flag of ADC FIFO has been underflow
static inline void ADC_ClearUnder(void) { } // HOST: no hardware

// clear flag of ADC FIFO has been overflow
static inline void ADC_ClearOver(void) { } // HOST: no hardware

// get number of samples waiting in FIFO
static inline u8 ADC_Level(void) { return (u8)0; } // HOST: no hardware

// get value from ADC FIFO (bit 15 can contain error flag)
static inline u16 ADC_Fifo(void) { return (u16)0; } // HOST: no hardware

// get value from ADC FIFO and wait to have data (bit 15 can contain error flag)
u16 ADC_FifoWait(void);

// discard FIFO results
void ADC_FifoFlush(void);

// get raw interrupt flag (= FIFO reaches threshold level)
static inline Bool ADC_IntRaw(void) { return (Bool)0; } // HOST: no hardware

// enable FIFO interrupt
static inline void ADC_IntEnable(void) { } // HOST: no hardware

// disable FIFO interrupt
static inline void ADC_IntDisable(void) { } // HOST: no hardware

// force FIFO interrupt request
static inline void ADC_Force(void) { } // HOST: no hardware

// clear forced FIFO interrupt request
static inline void ADC_ForceClear(void) { } // HOST: no hardware

// check if force FIFO interrupt request is set (can be reset with ADC_ForceClear())
static inline Bool ADC_IsForced(void) { return (Bool)0; } // HOST: no hardware

// get interrupt status after masking and forcing
static inline Bool ADC_IntStatus(void) { return (Bool)0; } // HOST: no hardware

// get current temperature in °C
float ADC_Temp();

// ----------------------------------------------------------------------------
//                          Original-SDK interface
// ----------------------------------------------------------------------------

#if USE_ORIGSDK		// include interface of original-SDK

// ADC init
static inline void adc_init(void) { ADC_Init(); }

// initialize GPIO to use as an ADC pin
//  pin ... pin 26 to 29 or 40 to 47
static inline void adc_gpio_init(uint pin) { ADC_PinInit(pin); }

// select ADC input
//  input ... input 0 to 4 or 0 to 8 (use ADC_MUX_*)
static inline void adc_select_input(uint input) { ADC_Mux(input); }

// get currently selected ADC input (returns 0 to 4 or 0 to 8)
static inline uint adc_get_selected_input(void) { return ADC_GetMux(); }

// set round robin sampling selector
//  mask ... mask of bits B0 to B4 or B0 to B8 of inputs (use BIT(ADC_MUX_*))
//           set 0 to disable round robin sampling
static inline void adc_set_round_robin(uint mask) { ADC_RoundRobin(mask); }

// enable or disable temperature sensor
static inline void adc_set_temp_sensor_enabled(bool enable) { if (enable) ADC_TempEnable(); else ADC_TempDisable(); }

// do single conversion
static inline u16 adc_read(void) { return ADC_Single(); }

// enable continuously repeated conversion (free-running sampling mode)
static inline void adc_run(bool run) { if (run) ADC_MultiEnable(); else ADC_MultiDisable(); }

// set ADC clock divisor as float (with resolution 1/256)
//  clkdiv ... clock divisor (if non-zero, ADC_MultiEnable() will start conversions with that interval)
// Interval of samples will be (1 + clkdiv) cycles. One conversion takes 96 cycles, so minimal value
// of clkdiv is 96 (period 2 us, 500 kHz). Maximal value is 65535 (period 1365 us, 732 Hz).
static inline void adc_set_clkdiv(float clkdiv) { ADC_ClkDivFloat(clkdiv); }

// setup ADC FIFO (FIFO is 4 samples long)
//  en ... True to enable write each conversion result to the FIFO
//  dreq_en ... enable DMA requests when FIFO contains data
//  dreq_thresh ... threshold for DMA requests/FIFO IRQ if enabled (DREQ/IRQ asserted then level >= threshold)
//  err_in_fifo ... True means bit 15 of FIFO contains error flag for each sample
//  shift ... True to right-shift result to be one byte 8-bit in size (enables DMA to byte buffers)
static inline void adc_fifo_setup(bool en, bool dreq_en, u16 dreq_thresh, bool err_in_fifo, bool byte_shift)
	{ ADC_FifoSetup(en, byte_shift, err_in_fifo, dreq_en, dreq_thresh); }

// check if ADC FIFO is empty
static inline bool adc_fifo_is_empty(void) { return ADC_IsEmpty(); }

// get number of samples waiting in FIFO
static inline u8 adc_fifo_get_level(void) { return ADC_Level(); }

// get result value from ADC FIFO (bit 15 can contain error flag)
static inline u16 adc_fifo_get(void) { return ADC_Fifo(); }

// get value from ADC FIFO and wait to have data (bit 15 can contain error flag)
static inline u16 adc_fifo_get_blocking(void) { return ADC_FifoWait(); }

// discard FIFO results
static inline void adc_fifo_drain(void) { ADC_FifoFlush(); }

// enable/disable ADC interrupt
static inline void adc_irq_set_enabled(bool enabled) { } // HOST: no hardware

#endif // USE_ORIGSDK

#ifdef __cplusplus
}
#endif

#endif // USE_ADC

#endif // _HOST_ADC_H
