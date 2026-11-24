/* PWM sound capture: PWMSndIrq (SDK, unchanged) driven by host_audio_render. */
#include "_sdk/inc/sdk_watchdog.h"
#include "_lib/inc/lib_pwmsnd.h"

#include "check.h"
#include "plat_fake.h"

void* thr_start(void (*fn)(void*), void* arg);
int thr_join(void* t, int timeout_ms);

static u8 snd[4410]; /* 0.2 s @22050 Hz */

static volatile int render_done;
static volatile int render_nonzero;

static void render_thread(void* arg)
{
    short b[64];
    int i, j;
    (void)arg;
    for (i = 0; i < 10000; i++) {
        host_audio_render(b, 64);
        for (j = 0; j < 64; j++) if (b[j] != 0) render_nonzero = 1;
    }
    render_done = 1;
}

static void audio_game(void)
{
    short buf[512];
    int i, n;
    void* th;

    for (i = 0; i < (int)sizeof(snd); i++) snd[i] = 0xC0;

    /* silence before any sound / PWM_Enable */
    host_audio_render(buf, 512);
    for (i = 0; i < 512; i++) CHECK_EQ(buf[i], 0);
    CHECK(!PlayingSound());

    PlaySound(snd, sizeof(snd));
    CHECK(PlayingSound());
    host_audio_render(buf, 512);
    CHECK(buf[100] > 0); /* 0xC0 > 0x80 midpoint */
    CHECK(buf[500] > 0);

    /* 4410 samples @22050 Hz = 8820 frames @44100: still playing after 8704 */
    for (n = 512; n + 512 <= 8704; n += 512) host_audio_render(buf, 512);
    CHECK_EQ(n, 8704);
    CHECK(PlayingSound());
    host_audio_render(buf, 512); /* 9216 >= 8820 */
    CHECK(!PlayingSound());
    host_audio_render(buf, 512);
    /* idle mixer sample is TOP/2 = 1023 -> -32, inaudible DC; no sound pulses */
    for (i = 0; i < 512; i++) CHECK(buf[i] >= -32 && buf[i] <= 0);

    /* concurrency: audio thread renders while the game thread calls PlaySound */
    render_done = 0;
    th = thr_start(render_thread, 0);
    for (i = 0; i < 1000; i++) PlaySound(snd, 64);
    CHECK(thr_join(th, 20000));
    CHECK(render_done);
    CHECK(render_nonzero);
    for (i = 0; i < 10 && PlayingSound(); i++) host_audio_render(buf, 64);
    CHECK(!PlayingSound());

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
