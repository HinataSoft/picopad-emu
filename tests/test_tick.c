/* plat_tick hook: called before every SysTick, can end the game with an exit code. */
#include "_sdk/inc/sdk_systick.h"
#include "_sdk/inc/sdk_watchdog.h"
#include "_devices/picopad/picopad_key.h"

#include "check.h"
#include "plat_fake.h"

static int tick_fn(unsigned long long t)
{
    if (t == 100000ULL) fake_keys = PLAT_KEY_BIT(KEY_A);
    if (t == 200000ULL) return 7;
    return -1;
}

static void tick_game(void)
{
    int i;
    while (Time() < 50000) WaitMs(5);
    for (i = 0; i < fake_tick_count && i < 64; i++)
        CHECK_EQ(fake_tick_times[i], 5000ULL * (i + 1));   /* every tick, from host_run_game start */
    CHECK(fake_tick_count >= 10);
    while (Time() < 100000) { CHECK_EQ(KeyGet(), NOKEY); WaitMs(5); }
    WaitMs(20);                             /* A set in tick 100000 before KeyScan, debounce settles */
    CHECK_EQ(KeyGet(), KEY_A);
    WaitMs(200);                            /* returns 7 at tick 200000 */
    CHECK(0);                               /* not reached */
}

int host_call_game_main(void)
{
    tick_game();
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 0 };
    host_configure(&cfg);
    fake_tick_fn = tick_fn;
    CHECK_EQ(host_run_game(), 7);
    CHECK_EQ(fake_tick_times[0], 5000ULL);
    /* second run: state is reset, ticks start again from 5000 */
    fake_tick_count = 0;
    fake_keys = 0;
    CHECK_EQ(host_run_game(), 7);
    CHECK_EQ(fake_tick_times[0], 5000ULL);
    return CHECK_DONE();
}
