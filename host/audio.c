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
//  - host_audio_pump() runs under the pump lock: on the game thread (called
//    from host_pump(), not while the game holds IRQ_LOCK) or in a stall step
//    of host_watchdog (holding IRQ_LOCK), never both at once. It is driven by
//    emulated time: after a pump exactly floor(host_now_us() * rate / 1e6) samples were produced since
//    host_run_game start (plus AUDIO_TARGET lead with PLAT_AUDIO_RING). Each
//    sample is one call of the stored IRQ_PWM_WRAP handler, converted to s16
//    ((samp-1024)*32). Without PWM_Enable or handler the samples are 0, so time
//    keeps flowing and PlayingSound() finishes even without an audio device.
//    Samples go to plat_audio_write (PLAT_AUDIO_CAPTURE), to a single-producer/
//    single-consumer ring (PLAT_AUDIO_RING, written only while it holds less
//    than 2*AUDIO_TARGET samples, else dropped) or nowhere (audio_out == 0);
//  - host_audio_render() (audio thread) only drains the ring with atomics and
//    never calls SDK code. Underrun repeats the last value, silence (0)
//    before anything was produced.
// The mixer rate is audio_rate, 0 meaning 44100.

#include "host_internal.h"

#define HOST_IRQ_MAX	32	// IRQ_NUM
#define HOST_PWM_SLICES	8
#define HOST_IRQ_PWM_WRAP 4	// IRQ_PWM_WRAP (sdk_irq_rp2040.h)

#define AUDIO_RING	8192	// ring size [frames], power of 2
#define AUDIO_TARGET	2048	// ring lead before the audio device [frames] (~46 ms @44100)
#define AUDIO_RATE_DEF	44100	// mixer rate when audio_rate == 0
#define AUDIO_CAPTURE_CHUNK 256	// samples per plat_audio_write call

// sound output slice/channel chosen by PWMSndInit (lib_pwmsnd.c)
extern int PWMSndSlice;
extern int PWMSndChan;

static void (* volatile irq_handlers[HOST_IRQ_MAX])(void);
static volatile int pwm_enabled[HOST_PWM_SLICES];
static volatile u32 snd_comp = 1024;	// last compare value of the sound channel

static short ring[AUDIO_RING];
static unsigned int ring_head;		// written under the pump lock only (atomic)
static unsigned int ring_tail;		// written by the audio thread only (atomic)
static short last_out;			// audio thread only: last delivered value
static unsigned long long produced;	// samples produced since host_run_game start (pump lock)

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

// under the pump lock (game thread or watchdog): produce the samples that are
// due by emulated time; 'produced' and the ring head are only touched here
void host_audio_pump(void)
{
	const host_config* cfg = host_cfg();
	void (*fn)(void);
	short cap[AUDIO_CAPTURE_CHUNK];
	unsigned long long due;
	unsigned int head, tail;
	int rate, out, ncap = 0, mix, slice;

	rate = cfg->audio_rate > 0 ? cfg->audio_rate : AUDIO_RATE_DEF;
	out = cfg->audio_out;
	due = host_now_us() * (unsigned long long)rate / 1000000ULL;
	if (out & PLAT_AUDIO_RING) due += AUDIO_TARGET;

	// without PWM_Enable or without a handler the samples are 0, time still flows
	slice = PWMSndSlice;
	fn = irq_handlers[HOST_IRQ_PWM_WRAP];
	mix = (fn != 0) && slice >= 0 && slice < HOST_PWM_SLICES && pwm_enabled[slice];

	head = ring_head;
	tail = __atomic_load_n(&ring_tail, __ATOMIC_ACQUIRE);
	while (produced < due)
	{
		int s = 0;
		if (mix)
		{
			fn();
			s = ((int)snd_comp - 1024) * 32;
			if (s > 32767) s = 32767;
			if (s < -32768) s = -32768;
		}
		if (out & PLAT_AUDIO_CAPTURE)
		{
			cap[ncap++] = (short)s;
			if (ncap == AUDIO_CAPTURE_CHUNK) { plat_audio_write(cap, ncap); ncap = 0; }
		}
		if (out & PLAT_AUDIO_RING)
		{
			// bounded latency: drop the sample when the ring is full enough
			if (head - tail < 2 * AUDIO_TARGET)
			{
				ring[head & (AUDIO_RING - 1)] = (short)s;
				head++;
				__atomic_store_n(&ring_head, head, __ATOMIC_RELEASE);
			}
			tail = __atomic_load_n(&ring_tail, __ATOMIC_ACQUIRE);
		}
		produced++;
	}
	if (ncap > 0) plat_audio_write(cap, ncap);
}

// start of a game run: the emulated time restarts, so does the sample counter
void host_audio_reset(void)
{
	produced = 0;
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
