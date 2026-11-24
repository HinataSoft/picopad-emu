// ****************************************************************************
//
//                       Host runtime - PWM sound capture
//
// ****************************************************************************
// The SDK mixer (PWMSndIrq, lib_pwmsnd.c) produces one sample per call and
// writes it with PWM_Comp(). On the device it runs in the PWM-wrap interrupt;
// here host_audio_render() (audio thread) calls the registered handler once
// per output frame and converts the compare value (0..2047, 1024 = silence)
// to s16 as (samp-1024)*32.
//
// The whole render call runs under host_irq_lock(): one lock per call, not per
// sample. The lock is recursive and cheap, but a per-sample lock would only
// make the audio thread contend with the game thread's IRQ_LOCK sections 512x
// more often; one buffer is ~12 ms at most of mixer work (microseconds in
// practice), the same granularity as an interrupt burst, and the mixer state
// (PwmSound[]) is then consistent for the whole buffer.

#include "host_internal.h"

#define HOST_IRQ_MAX	32	// IRQ_NUM
#define HOST_PWM_SLICES	8
#define HOST_IRQ_PWM_WRAP 4	// IRQ_PWM_WRAP (sdk_irq_rp2040.h)

static void (* volatile irq_handlers[HOST_IRQ_MAX])(void);
static volatile u32 pwm_comp[HOST_PWM_SLICES][2];
static volatile int pwm_enabled[HOST_PWM_SLICES];

// samples written during a handler call (only touched under the IRQ lock)
static int in_handler;
static u32 handler_sample;

void host_set_handler(int irq, void (*fn)(void))
{
	if (irq < 0 || irq >= HOST_IRQ_MAX) return;
	irq_handlers[irq] = fn;
}

void host_pwm_comp(int slice, int chan, u32 val)
{
	if (slice < 0 || slice >= HOST_PWM_SLICES || (chan & ~1)) return;
	pwm_comp[slice][chan] = val;
	// the mixer writes R (if any) first and the main channel last: the last
	// compare value written inside the handler is the mono output sample
	if (in_handler) handler_sample = val;
}

void host_pwm_enable(int slice, int on)
{
	if (slice < 0 || slice >= HOST_PWM_SLICES) return;
	pwm_enabled[slice] = on ? 1 : 0;
}

static int any_pwm_enabled(void)
{
	int i;
	for (i = 0; i < HOST_PWM_SLICES; i++) if (pwm_enabled[i]) return 1;
	return 0;
}

void host_audio_render(short* out, int frames)
{
	int i;
	void (*fn)(void) = irq_handlers[HOST_IRQ_PWM_WRAP];

	// no handler or PWM never enabled (mixer enables it on first sound): silence
	if (!fn || !any_pwm_enabled()) {
		for (i = 0; i < frames; i++) out[i] = 0;
		return;
	}

	host_irq_lock();
	for (i = 0; i < frames; i++) {
		int s;
		// handler may have been replaced/cleared meanwhile
		fn = irq_handlers[HOST_IRQ_PWM_WRAP];
		if (!fn || !any_pwm_enabled()) { out[i] = 0; continue; }
		handler_sample = 1024;
		in_handler = 1;
		fn();
		in_handler = 0;
		s = ((int)handler_sample - 1024) * 32;
		if (s > 32767) s = 32767;
		if (s < -32768) s = -32768;
		out[i] = (short)s;
	}
	host_irq_unlock();
}
