/* PWM sound: the SDK mixer (PWMSndIrq, unchanged) runs on the game thread via
   the pump into a ring buffer; host_audio_render only drains it. */
#include "_sdk/inc/sdk_watchdog.h"
#include "_sdk/inc/sdk_irq.h"
#include "_lib/inc/lib_pwmsnd.h"

#include "check.h"
#include "plat_fake.h"

void* thr_start(void (*fn)(void*), void* arg);
int thr_join(void* t, int timeout_ms);
int host_is_game_thread(void);
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

static void audio_game(void)
{
    short buf[512];
    int i, n;
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
    WaitMs(10);
    host_audio_render(buf, 512);
    CHECK(buf[100] > 0); /* 0xC0 > 0x80 midpoint */
    CHECK(buf[500] > 0);

    /* 4410 samples @22050 Hz = 8820 frames @44100. The pump keeps 2048
       frames ahead, so produced = rendered + 2048 (after the pump). */
    for (n = 512; n + 512 <= 4096; n += 512) {
        WaitMs(1);
        host_audio_render(buf, 512);
    }
    WaitMs(1);
    CHECK_EQ(n, 4096);
    CHECK(PlayingSound()); /* ~6144 produced, well before 8820 */
    for (; n < 9216; n += 512) {
        WaitMs(1);
        host_audio_render(buf, 512);
    }
    WaitMs(1); /* >= 9216 + 2048 produced */
    CHECK(!PlayingSound());
    WaitMs(10);
    host_audio_render(buf, 512);
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

    ResetToBootLoader();
}

int host_call_game_main(void)
{
    audio_game();
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 44100 };
    host_configure(&cfg);
    CHECK_EQ(host_run_game(), 0);
    return CHECK_DONE();
}
