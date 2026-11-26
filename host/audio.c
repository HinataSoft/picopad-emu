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
//    host_run_game start (plus a lead with PLAT_AUDIO_RING, see below). Each
//    sample is one call of the stored IRQ_PWM_WRAP handler, converted to s16
//    ((samp-1024)*32). Without PWM_Enable or handler the samples are 0, so time
//    keeps flowing and PlayingSound() finishes even without an audio device.
//    Samples go to plat_audio_write (PLAT_AUDIO_CAPTURE), to a single-producer/
//    single-consumer ring (PLAT_AUDIO_RING, written only while it holds less
//    than 2*AUDIO_TARGET samples, else dropped) or nowhere (audio_out == 0);
//  - host_audio_render() (audio thread) only drains the ring with atomics and
//    never calls SDK code. Underrun repeats the last value, silence (0)
//    before anything was produced.
// Drift correction (PLAT_AUDIO_RING only): production follows the emulated
// (QPC-based) clock, the audio device consumes on its own crystal. The lead
// starts at AUDIO_TARGET and moves by one sample per AUDIO_ADJ_SAMPLES of
// emulated time (at most AUDIO_ADJ_MAX_STEP per pump, so a long gap between
// pumps does not jump it) while the ring level is below AUDIO_LOW (lead up)
// or between AUDIO_HIGH and the 2*AUDIO_TARGET cap (lead down; at the cap the
// consumer is stalled, not drifting, and the samples are dropped instead).
// The level thus stays in [AUDIO_LOW, AUDIO_HIGH] for any drift up to
// 1/AUDIO_ADJ_SAMPLES (~3 %). PLAT_AUDIO_CAPTURE (headless/WAV) is unaffected:
// exactly floor(t * rate / 1e6) samples, deterministic.
// The mixer rate is audio_rate, 0 meaning 44100.
// GPIO sound (bit-bang, e.g. the TINYGAME games with USE_PWMSND 0 toggle
// PWMSND_GPIO with gpio_put and time it with WaitUs): until PWM_Enable was
// called on the sound slice in this run (sticky, pwm_snd_used), a sample is
// the fraction of its interval the pin spent at 1, times GPIO_SND_ONE (idle 0
// = silence 0, no DC offset). Time is scaled by the rate (t_us * rate), so
// sample g covers [g, g+1) * 1e6 exactly as the floor(t * rate / 1e6) count.
// host_gpio_snd_level (game thread) takes the pump lock, lets the mixer catch
// up to host_now_us() with the old level, then records the change: the cursor
// (gpio_idx: sample being accumulated, gpio_seg: start of the current level
// inside it, gpio_hi: time at 1 before gpio_seg) only moves forward. With
// PLAT_AUDIO_RING production runs 'lead' samples ahead of emulated time, so
// produced sample p takes GPIO sample p - lead (the lead is latency, as for
// the PWM mixer; when it changes by a step, a GPIO sample repeats or drops).
// The GPIO state is touched only under the pump lock (watchdog steps run the
// mixer too), except gpio_level after pwm_snd_used (then unused by the mixer).

#include "host_internal.h"

#define HOST_IRQ_MAX	32	// IRQ_NUM
#define HOST_PWM_SLICES	8
#define HOST_IRQ_PWM_WRAP 4	// IRQ_PWM_WRAP (sdk_irq_rp2040.h)

#define AUDIO_RING	8192	// ring size [frames], power of 2
#define AUDIO_TARGET	2048	// ring lead before the audio device [frames] (~46 ms @44100)
#define AUDIO_RATE_DEF	44100	// mixer rate when audio_rate == 0
#define AUDIO_CAPTURE_CHUNK 256	// samples per plat_audio_write call
#define AUDIO_LOW	1024	// ring level below which the lead grows [frames]
#define AUDIO_HIGH	3072	// ring level above which the lead shrinks [frames]
#define AUDIO_ADJ_SAMPLES 32	// emulated samples per lead step of 1 sample (max drift ~3 %)
#define AUDIO_ADJ_MAX_STEP 8	// max lead change per pump [frames]
#define GPIO_SND_ONE	16384	// GPIO sound: sample value of a whole interval at level 1
#define GPIO_SND_UNIT	1000000ULL // GPIO sound: one sample in scaled time (t_us * rate)

// sound output slice/channel chosen by PWMSndInit (lib_pwmsnd.c, compiled
// into the game only with USE_PWMSND; without it there is no PWM sound)
#if USE_PWMSND
extern int PWMSndSlice;
extern int PWMSndChan;
#else
#define PWMSndSlice	(-1)
#define PWMSndChan	(-1)
#endif

static void (* volatile irq_handlers[HOST_IRQ_MAX])(void);
static volatile int pwm_enabled[HOST_PWM_SLICES];
static volatile u32 snd_comp = 1024;	// last compare value of the sound channel

static short ring[AUDIO_RING];
static unsigned int ring_head;		// written under the pump lock only (atomic)
static unsigned int ring_tail;		// written by the audio thread only (atomic)
static short last_out;			// audio thread only: last delivered value
static unsigned long long produced;	// samples produced since host_run_game start (pump lock)
static long long lead = AUDIO_TARGET;	// ring mode: production ahead of emulated time [frames] (pump lock)
static unsigned long long adj_base;	// ring mode: emulated sample time of the last lead step (pump lock)

static volatile int pwm_snd_used;	// 1 = PWM_Enable on the sound slice in this run: no GPIO sound
static int gpio_level;			// level of PWMSND_GPIO (game thread writes, pump lock)
static unsigned long long gpio_idx;	// GPIO sample being accumulated (pump lock)
static unsigned long long gpio_seg;	// scaled time since which gpio_level holds, within gpio_idx (pump lock)
static unsigned long long gpio_hi;	// scaled time at 1 inside gpio_idx before gpio_seg (pump lock)
static short gpio_last;			// last GPIO sample, repeated when the index steps back (pump lock)

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
	if (on && slice == PWMSndSlice) pwm_snd_used = 1;	// the SDK PWM sound runs
}

static void audio_pump_at(unsigned long long now);

// GPIO sound sample g (pump lock): level history from the cursor to the end
// of sample g (in the past or at the current level, see host_audio_pump)
static short gpio_sample(unsigned long long g)
{
	unsigned long long end, hi;
	if (g < gpio_idx) return gpio_last;	// ring lead grew: repeat
	if (g > gpio_idx)
	{
		// ring lead shrank: drop the samples in between; no change was
		// recorded after gpio_seg, so the level holds over all of sample g
		gpio_idx = g;
		gpio_hi = 0;
		gpio_seg = g * GPIO_SND_UNIT;
	}
	end = (g + 1) * GPIO_SND_UNIT;
	hi = gpio_hi + (gpio_level ? end - gpio_seg : 0);
	gpio_last = (short)(hi * GPIO_SND_ONE / GPIO_SND_UNIT);
	gpio_idx = g + 1;
	gpio_hi = 0;
	gpio_seg = end;
	return gpio_last;
}

void host_gpio_snd_level(int level)
{
	unsigned long long now, t, cur;
	int rate, took;

	level = level ? 1 : 0;
	if (!host_is_game_thread() || level == gpio_level) return;
	if (pwm_snd_used)
	{
		gpio_level = level;	// PWM path: the mixer does not read it
		return;
	}

	took = host_pump_lock();
	now = host_now_us();
	audio_pump_at(now);	// samples up to now with the old level

	rate = host_cfg()->audio_rate > 0 ? host_cfg()->audio_rate : AUDIO_RATE_DEF;
	t = now * (unsigned long long)rate;
	cur = t / GPIO_SND_UNIT;
	if (cur > gpio_idx)
	{
		// the mixer has not taken the samples before 'cur' (the ring lead
		// shrank or is below zero): they are dropped, sample 'cur' starts at
		// the old level
		gpio_idx = cur;
		gpio_hi = 0;
		gpio_seg = cur * GPIO_SND_UNIT;
	}
	if (t > gpio_seg)
	{
		if (gpio_level) gpio_hi += t - gpio_seg;
		gpio_seg = t;
	}
	// else: the cursor is ahead of 't' (after a lead step), the change
	// takes effect at gpio_seg
	gpio_level = level;

	if (took) host_pump_unlock();
}

int host_gpio_snd_get(void)
{
	return gpio_level;
}

// under the pump lock (game thread or watchdog): produce the samples that are
// due by emulated time; 'produced' and the ring head are only touched here
void host_audio_pump(void)
{
	audio_pump_at(host_now_us());
}

// host_audio_pump for emulated time 'now' [us]
static void audio_pump_at(unsigned long long now)
{
	const host_config* cfg = host_cfg();
	void (*fn)(void);
	short cap[AUDIO_CAPTURE_CHUNK];
	unsigned long long due;
	unsigned int head, tail;
	int rate, out, ncap = 0, mix, slice, gpio;
	long long glag = 0;	// GPIO sample = produced - glag (ring lead)

	rate = cfg->audio_rate > 0 ? cfg->audio_rate : AUDIO_RATE_DEF;
	out = cfg->audio_out;
	due = now * (unsigned long long)rate / 1000000ULL;
	if (out & PLAT_AUDIO_RING)
	{
		// drift correction: steer the lead by the ring level
		unsigned long long steps = (due - adj_base) / AUDIO_ADJ_SAMPLES;
		unsigned int level = ring_head - __atomic_load_n(&ring_tail, __ATOMIC_ACQUIRE);
		if (steps > 0)
		{
			adj_base += steps * AUDIO_ADJ_SAMPLES;
			if (steps > AUDIO_ADJ_MAX_STEP) steps = AUDIO_ADJ_MAX_STEP;
			if (level < AUDIO_LOW) lead += (long long)steps;
			else if (level > AUDIO_HIGH && level < 2 * AUDIO_TARGET) lead -= (long long)steps;
		}
		if (lead >= 0) due += (unsigned long long)lead;
		else due = (due > (unsigned long long)-lead) ? due - (unsigned long long)-lead : 0;
		glag = lead;
	}

	// without PWM_Enable or without a handler the samples are 0, time still flows
	slice = PWMSndSlice;
	fn = irq_handlers[HOST_IRQ_PWM_WRAP];
	mix = (fn != 0) && slice >= 0 && slice < HOST_PWM_SLICES && pwm_enabled[slice];
	gpio = !mix && !pwm_snd_used;

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
		else if (gpio)
		{
			long long g = (long long)produced - glag;
			if (g >= 0) s = gpio_sample((unsigned long long)g);
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
	lead = AUDIO_TARGET;
	adj_base = 0;
	// after DeviceInit (PWMSndInit leaves the slice disabled): GPIO sound
	// until the game enables it
	pwm_snd_used = PWMSndSlice >= 0 && PWMSndSlice < HOST_PWM_SLICES && pwm_enabled[PWMSndSlice];
	gpio_level = 0;
	gpio_idx = 0;
	gpio_seg = 0;
	gpio_hi = 0;
	gpio_last = 0;
}

unsigned int host_audio_ring_level(void)
{
	return __atomic_load_n(&ring_head, __ATOMIC_ACQUIRE) - __atomic_load_n(&ring_tail, __ATOMIC_ACQUIRE);
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
