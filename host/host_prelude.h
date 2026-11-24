// ****************************************************************************
//
//          Host prelude - force-included (-include) into every SDK TU
//
// ****************************************************************************
// Replaces the hardware headers of PicoLibSDK by host versions without
// modifying the SDK: the include guards of the original headers are defined
// here first, so their later #include (from includes.h, _sdk/_include.h or
// directly from SDK sources) is skipped, and the host replacements
// host/hw/host_*.h are included instead. They provide the same names,
// constants and macros, but their inline bodies never touch registers or
// use ARM assembler; hardware behaviour is delegated to host_* functions
// implemented by the emulator (host/*.c).
//
// Must not include SDL, windows.h or <stdint.h> (see global constraints).

#ifndef _HOST_PRELUDE_H
#define _HOST_PRELUDE_H

// guards of replaced SDK headers (_sdk/inc/sdk_*.h)
#define _SDK_CPU_H
#define _SDK_TIMER_H
#define _SDK_SPINLOCK_H
#define _SDK_GPIO_H
#define _SDK_PWM_H
#define _SDK_IRQ_H
#define _SDK_ADC_H
#define _SDK_DMA_H

// MinGW math.h does not declare the obsolete GNU function drem(), used by
// _sdk/inc/sdk_double.h (dremd); it is identical to remainder()
#define drem remainder

// attribute shorthands from newlib <sys/cdefs.h> (ARM toolchain), used by
// _sdk/inc/sdk_pio.h (pio_program_t) and _sdk/inc/sdk_spi.h; MinGW has none
#ifndef __packed
#define __packed __attribute__((packed))
#endif
#ifndef __unused
#define __unused __attribute__((unused))
#endif

// MinGW <limits.h> defines PATH_MAX 260, lib_fat.h redefines it to 254
// without #undef (warning). Include <limits.h> first and drop its value, so
// PATH_MAX is always the SDK one, whatever the include order.
#include <limits.h>
#undef PATH_MAX

// SDK base definitions and configuration (config.h of the game)
#include "global.h"

// host replacements of the hardware headers
#include "hw/host_cpu.h"
#include "hw/host_irq.h"
#include "hw/host_timer.h"
#include "hw/host_spinlock.h"
#include "hw/host_gpio.h"
#include "hw/host_pwm.h"
#include "hw/host_adc.h"
#include "hw/host_dma.h"

#endif // _HOST_PRELUDE_H
