/* PWM sound: the SDK mixer (PWMSndIrq, unchanged) runs on the game thread via
   the pump, driven by emulated time; samples go to plat_audio_write (capture),
   to a ring drained by host_audio_render, or are discarded. */
#include "_sdk/inc/sdk_watchdog.h"
#include "_sdk/inc/sdk_irq.h"
#include "_lib/inc/lib_pwmsnd.h"

#include <stdio.h>

#include "check.h"
#include "plat_fake.h"

void* thr_start(void (*fn)(void*), void* arg);
int thr_join(void* t, int timeout_ms);
int host_is_game_thread(void);
unsigned int host_audio_ring_level(void);
void PWMSndIrq(void);

static u8 snd[4410]; /* 0.2 s @22050 Hz */

static volatile int off_thread_calls;
static volatile int handler_calls;
static void probe_handler(void)
{
    handler_calls++;
    if (!host_is_game_thread()) off_thread_calls++;
    PWMSndIrq();
}

static volatile int render_stop;
static volatile int render_count;
static void render_thread(void* arg)
{
    short b[64];
    (void)arg;
    while (!render_stop) {
        host_audio_render(b, 64);
        render_count++;
    }
}

static int run_mode; /* 0 = capture, 1 = discard (audio_out 0), 2 = ring + render thread,
                        3 = ring, consumer clock faster/slower than emulated time */

#define DUE(t) ((int)((unsigned long long)(t) * 44100 / 1000000))

/* audio_out = PLAT_AUDIO_CAPTURE, virtual time: production follows emulated time */
static void audio_capture(void)
{
    int i, c0;
    u64 t0;

    for (i = 0; i < (int)sizeof(snd); i++) snd[i] = 0xC0;
    t0 = Time64(); /* RuntimeInit/DeviceInit may already have advanced time */
    CHECK_EQ(fake_audio_count, DUE(t0));
    c0 = fake_audio_count;
    PlaySound(snd, sizeof(snd)); /* 4410 B @22050 Hz = 0.2 s */
    WaitMs(100);
    CHECK_EQ(fake_audio_count, DUE(t0 + 100000));
    CHECK(PlayingSound());
    WaitMs(150);
    CHECK(!PlayingSound()); /* no audio device needed */
    CHECK_EQ(fake_audio_count, DUE(t0 + 250000));
    CHECK(fake_audio[c0 + 100] > 0); /* 0xC0 samples (fake_audio keeps the first 65536) */
}

/* audio_out = 0: the mixer runs, samples are discarded */
static void audio_discard(void)
{
    int i;
    for (i = 0; i < (int)sizeof(snd); i++) snd[i] = 0xC0;
    PlaySound(snd, sizeof(snd));
    CHECK(PlayingSound());
    WaitMs(250);
    CHECK(!PlayingSound());
    CHECK_EQ(fake_audio_count, 0);
}

/* audio_out = PLAT_AUDIO_RING, real time (fake_now) */
static void audio_game(void)
{
    short buf[512];
    int i, n, seen;
    void* th;

    for (i = 0; i < (int)sizeof(snd); i++) snd[i] = 0xC0;

    /* wrap the mixer handler to see which thread runs it */
    SetHandler(IRQ_PWM_WRAP, probe_handler);

    /* silence before any sound, even after pumping */
    WaitMs(10);
    host_audio_render(buf, 512);
    for (i = 0; i < 512; i++) CHECK_EQ(buf[i], 0);
    CHECK(!PlayingSound());
    CHECK_EQ(handler_calls, 0); /* PWM not enabled yet: mixer not run */

    PlaySound(snd, sizeof(snd));
    CHECK(PlayingSound());
    /* the ring holds ~2048 frames of earlier silence: the sound follows */
    seen = 0;
    for (n = 0; n < 16; n++) {
        WaitMs(10);
        host_audio_render(buf, 512);
        if (buf[100] > 0 || buf[500] > 0) seen = 1;
    }
    CHECK(seen); /* 0xC0 > 0x80 midpoint */
    CHECK(PlayingSound()); /* 8820 frames = 200 ms, only ~160 ms elapsed */
    for (n = 0; n < 2000 && PlayingSound(); n++) WaitMs(5);
    CHECK(!PlayingSound());
    WaitMs(100);
    for (n = 0; n < 16; n++) host_audio_render(buf, 512);
    /* idle mixer sample is TOP/2 = 1023 -> -32, inaudible DC */
    for (i = 0; i < 512; i++) CHECK(buf[i] >= -32 && buf[i] <= 0);

    /* concurrency: audio thread drains continuously while the game thread
       alternates sounds of different lengths */
    render_stop = 0;
    render_count = 0;
    th = thr_start(render_thread, 0);
    while (render_count < 1) { }
    for (i = 0; i < 1000; i++) {
        PlaySound(snd, (i & 1) ? 3000 : 64);
        if (i % 7 == 0) StopSound();
        WaitMs(1);
    }
    for (i = 0; i < 2000 && PlayingSound(); i++) WaitMs(5);
    CHECK(!PlayingSound());
    render_stop = 1;
    CHECK(thr_join(th, 20000));
    CHECK(render_count > 1);
    CHECK(handler_calls > 0);
    CHECK_EQ(off_thread_calls, 0);
}

/* mode 3: the audio device runs on its own crystal, 'drift_ppm' faster
   (> 0) or slower (< 0) than the emulated (QPC) time that drives production.
   Over 25 minutes of exaggerated drift the ring level must stay bounded
   around AUDIO_TARGET (2048): no sustained underrun, no pinning at the cap. */
static int drift_ppm;
static unsigned int drift_min = 99999, drift_max, drift_underruns;
static void audio_drift(void)
{
    static short buf[1024];
    const unsigned long long span_us = 25ull * 60 * 1000000;   /* 25 min */
    const unsigned long long warm_us = 5ull * 1000000;         /* fill + settle */
    unsigned long long t0 = fake_now, consumed = 0;
    for (;;) {
        unsigned long long el, due;
        unsigned int lvl;
        int n;
        WaitMs(5);
        el = fake_now - t0;
        /* consumer: el * 44100 * (1 + ppm/1e6) samples so far */
        due = el * 44100ull / 1000000ull;
        if (drift_ppm >= 0) due += el * 44100ull / 1000000ull * (unsigned)drift_ppm / 1000000ull;
        else due -= el * 44100ull / 1000000ull * (unsigned)(-drift_ppm) / 1000000ull;
        n = (int)(due - consumed);
        if (n > 1024) n = 1024;
        if (el >= warm_us && host_audio_ring_level() < (unsigned)n) drift_underruns++;
        host_audio_render(buf, n);
        consumed += (unsigned)n;
        lvl = host_audio_ring_level();
        if (el >= warm_us) {
            if (lvl < drift_min) drift_min = lvl;
            if (lvl > drift_max) drift_max = lvl;
        }
        if (el >= span_us) break;
    }
}

int host_call_game_main(void)
{
    if (run_mode == 0) audio_capture();
    else if (run_mode == 1) audio_discard();
    else if (run_mode == 3) audio_drift();
    else audio_game();
    ResetToBootLoader();
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 44100,
                        .audio_out = PLAT_AUDIO_CAPTURE };
    host_configure(&cfg);
    run_mode = 0;
    fake_audio_count = 0;
    CHECK_EQ(host_run_game(), 0);

    cfg.audio_out = 0;
    host_configure(&cfg);
    run_mode = 1;
    fake_audio_count = 0;
    CHECK_EQ(host_run_game(), 0);

    cfg.virtual_clock = 0;
    cfg.audio_out = PLAT_AUDIO_RING;
    host_configure(&cfg);
    run_mode = 2;
    CHECK_EQ(host_run_game(), 0);

    /* drift: consumer 1.5 % faster, then 1.5 % slower than production */
    run_mode = 3;
    drift_ppm = 15000;
    drift_min = 99999; drift_max = 0; drift_underruns = 0;
    CHECK_EQ(host_run_game(), 0);
    printf("drift +1.5%%: level %u..%u, underruns %u\n", drift_min, drift_max, drift_underruns);
    CHECK(drift_min >= 512);
    CHECK(drift_max <= 3584);
    CHECK_EQ(drift_underruns, 0);

    drift_ppm = -15000;
    drift_min = 99999; drift_max = 0; drift_underruns = 0;
    CHECK_EQ(host_run_game(), 0);
    printf("drift -1.5%%: level %u..%u, underruns %u\n", drift_min, drift_max, drift_underruns);
    CHECK(drift_min >= 512);
    CHECK(drift_max <= 3584);
    CHECK_EQ(drift_underruns, 0);
    return CHECK_DONE();
}
