// ****************************************************************************
//
//                       Host runtime - PWM sound capture
//
// ****************************************************************************
// The SDK mixer (PWMSndIrq, lib_pwmsnd.c) produces one sample per call and
// writes it with PWM_Comp(). On the device it runs in the PWM-wrap interrupt,
// preempting the game on the same core. The SDK sound API (PlaySoundChan,
// StopSoundChan) is not thread-safe against a mixer running in parallel, so
// the host never runs the mixer on another thread:
//  - host_audio_pump() runs on the game thread (called from host_pump(), not
//    while the game holds IRQ_LOCK) and calls the stored IRQ_PWM_WRAP handler
//    once per output frame to top up a single-producer/single-consumer ring
//    of s16 samples ((samp-1024)*32) to AUDIO_TARGET frames;
//  - host_audio_render() (audio thread) only drains the ring with atomics and
//    never calls SDK code. Underrun repeats the last value, silence (0)
//    before anything was produced.
// Without an audio device (audio_rate == 0) nothing is mixed.

#include "host_internal.h"

#define HOST_IRQ_MAX	32	// IRQ_NUM
#define HOST_PWM_SLICES	8
#define HOST_IRQ_PWM_WRAP 4	// IRQ_PWM_WRAP (sdk_irq_rp2040.h)

#define AUDIO_RING	8192	// ring size [frames], power of 2
#define AUDIO_TARGET	2048	// fill level kept by the pump [frames] (~46 ms @44100)

// sound output slice/channel chosen by PWMSndInit (lib_pwmsnd.c)
extern int PWMSndSlice;
extern int PWMSndChan;

static void (* volatile irq_handlers[HOST_IRQ_MAX])(void);
static volatile int pwm_enabled[HOST_PWM_SLICES];
static volatile u32 snd_comp = 1024;	// last compare value of the sound channel

static short ring[AUDIO_RING];
static unsigned int ring_head;		// written by the game thread only (atomic)
static unsigned int ring_tail;		// written by the audio thread only (atomic)
static short last_out;			// audio thread only: last delivered value

void host_set_handler(int irq, void (*fn)(void))
{
	if (irq < 0 || irq >= HOST_IRQ_MAX) return;
	irq_handlers[irq] = fn;
}

void host_pwm_comp(int slice, int chan, u32 val)
{
	// only the sound channel of the mixer is captured (not e.g. backlight)
	if (slice == PWMSndSlice && chan == PWMSndChan) snd_comp = val;
}

void host_pwm_enable(int slice, int on)
{
	if (slice < 0 || slice >= HOST_PWM_SLICES) return;
	pwm_enabled[slice] = on ? 1 : 0;
}

// game thread: produce samples up to the target fill level
void host_audio_pump(void)
{
	void (*fn)(void);
	unsigned int head, tail;
	int slice;

	if (host_cfg()->audio_rate <= 0) return;
	slice = PWMSndSlice;
	if (slice < 0 || slice >= HOST_PWM_SLICES || !pwm_enabled[slice]) return;
	fn = irq_handlers[HOST_IRQ_PWM_WRAP];
	if (!fn) return;

	head = ring_head;
	tail = __atomic_load_n(&ring_tail, __ATOMIC_ACQUIRE);
	while (head - tail < AUDIO_TARGET)
	{
		int s;
		fn();
		s = ((int)snd_comp - 1024) * 32;
		if (s > 32767) s = 32767;
		if (s < -32768) s = -32768;
		ring[head & (AUDIO_RING - 1)] = (short)s;
		head++;
		__atomic_store_n(&ring_head, head, __ATOMIC_RELEASE);
	}
}

// audio thread: drain only, never calls SDK code
void host_audio_render(short* out, int frames)
{
	int i;
	unsigned int tail = ring_tail;
	unsigned int head = __atomic_load_n(&ring_head, __ATOMIC_ACQUIRE);

	for (i = 0; i < frames; i++)
	{
		if (head - tail == 0)
		{
			head = __atomic_load_n(&ring_head, __ATOMIC_ACQUIRE);
			if (head - tail == 0) { out[i] = last_out; continue; }	// underrun
		}
		last_out = ring[tail & (AUDIO_RING - 1)];
		tail++;
		out[i] = last_out;
	}
	__atomic_store_n(&ring_tail, tail, __ATOMIC_RELEASE);
}
