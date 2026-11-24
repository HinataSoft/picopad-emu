// ****************************************************************************
//
//                 Host runtime - audio (placeholder — replaced in Task 6)
//
// ****************************************************************************
// Minimal link stubs: IRQ handlers and PWM state are not stored, the rendered
// audio is silence.

#include "host_internal.h"

void host_set_handler(int irq, void (*fn)(void)) { (void)irq; (void)fn; }
void host_pwm_comp(int slice, int chan, u32 val) { (void)slice; (void)chan; (void)val; }
void host_pwm_enable(int slice, int on) { (void)slice; (void)on; }

void host_audio_render(short* out, int frames)
{
	int i;
	for (i = 0; i < frames; i++) out[i] = 0;
}
