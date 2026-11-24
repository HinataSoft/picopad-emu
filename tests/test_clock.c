/* Clock and SysTick pump: virtual clock (WaitMs/WaitUs, SysTime every 5 ms)
   and the busy-wait protection of the real clock (test_busy_wait). */
/* SDK headers one by one: includes.h (lib_print.h getchar) clashes with stdio.h */
#include "_sdk/inc/sdk_systick.h"  /* SysTime */
#include "_sdk/inc/sdk_watchdog.h" /* ResetToBootLoader */
#include "_lib/inc/lib_alarm.h"    /* sAlarm: observer called from the SysTick handler */

#include "check.h"
#include "plat_fake.h"

static int phase; /* 0 = virtual clock, 1 = busy wait, 2 = short waits, 3 = compute */
static unsigned long long busy_delta;
static unsigned long long short_emul_delta; /* emulated time of 100 x WaitUs(170) */
static unsigned long long short_real_delta; /* real (fake) time of the same */
static int short_sleeps;                    /* plat_sleep_us calls during it */
static int short_min_ok;                    /* every WaitUs(170) advanced Time64 >= 170 */
static int compute_ok;                      /* WaitUs(100) after computing advanced >= 100 */

static u32 alarm_times[16];
static int alarm_count;
static void alarm_cb(sAlarm* a)
{
    (void)a;
    if (alarm_count < 16) alarm_times[alarm_count++] = Time();
}

static void virtual_clock_game(void)
{
    /* virtual clock: WaitMs advances time exactly and runs SysTick every 5 ms */
    u32 t0 = Time();
    u32 s0 = SysTime;
    WaitMs(50);
    CHECK_EQ(Time() - t0, 50000);
    CHECK_EQ(SysTime - s0, 50);
    WaitUs(4999);
    CHECK_EQ(SysTime - s0, 50); /* not yet next tick */
    WaitUs(1);
    CHECK_EQ(SysTime - s0, 55);
    /* Time64 follows the same clock */
    CHECK_EQ(Time64() - t0, 55000);
    /* waits are advanced in steps of at most 5000 us: the SysTick observer sees
       evenly spaced Time() values instead of one final value */
    {
        static sAlarm al;
        int i;
        u32 t1;
        al.time = SysTime + 5;
        al.delta = 5;
        al.callback = alarm_cb;
        AlarmReg(&al);
        alarm_count = 0;
        t1 = Time();
        WaitUs(12000);
        CHECK_EQ(Time() - t1, 12000);
        CHECK_EQ(alarm_count, 2);
        for (i = 0; i < alarm_count; i++) CHECK_EQ(alarm_times[i] - t1, 5000 * (i + 1));
        AlarmUnreg(&al);
    }
    ResetToBootLoader(); /* host_run_game returns 0 */
    CHECK(0);            /* not reached */
}

static void busy_wait_game(void)
{
    /* real clock, time does not advance by itself: 2000 pumps without 1 ms
       elapsed must sleep at least once (200 us) */
    int i;
    unsigned long long before = fake_now;
    for (i = 0; i < 2000; i++) dmb();
    busy_delta = fake_now - before;
}

static void short_waits_game(void)
{
    /* real clock with a 530 us minimal OS sleep: 100 short waits (a 16x16
       DispUpdate costs 170 us) must take ~17 ms, not 100 x 530 us */
    int i;
    u64 t0 = Time64();
    unsigned long long r0 = fake_now;
    int s0 = fake_sleep_count;
    short_min_ok = 1;
    for (i = 0; i < 100; i++)
    {
        u64 t = Time64();
        WaitUs(170);
        if (Time64() - t < 170) short_min_ok = 0;
    }
    short_emul_delta = Time64() - t0;
    short_real_delta = fake_now - r0;
    short_sleeps = fake_sleep_count - s0;
}

static void compute_game(void)
{
    /* real clock: a short wait leaves a carried remainder, then the game
       computes ~3 ms without waiting (fake clock jumps); the next WaitUs(100)
       must still advance Time64 by >= 100 us (compute time is not forgiven) */
    int i;
    u64 t;
    compute_ok = 1;
    for (i = 0; i < 3; i++)
    {
        WaitUs(170);
        fake_now += 3000; /* computing */
        t = Time64();
        WaitUs(100);
        if (Time64() - t < 100) compute_ok = 0;
    }
}

int host_call_game_main(void)
{
    if (phase == 0)
        virtual_clock_game();
    else if (phase == 1)
        busy_wait_game();
    else if (phase == 2)
        short_waits_game();
    else
        compute_game();
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 0 };
    phase = 0;
    host_configure(&cfg);
    CHECK_EQ(host_run_game(), 0);

    /* test_busy_wait */
    host_config real = { .virtual_clock = 0, .fast_display = 1, .seed = 1, .audio_rate = 0 };
    phase = 1;
    fake_now = 1000000;
    host_configure(&real);
    CHECK_EQ(host_run_game(), 0); /* game_main returned normally */
    CHECK(busy_delta >= 200);

    /* short waits on the real clock: emulated time tracks the requested time */
    phase = 2;
    fake_now = 2000000;
    fake_sleep_min_us = 530;
    host_configure(&real);
    CHECK_EQ(host_run_game(), 0);
    fprintf(stderr, "short waits: emulated %llu us, real %llu us, %d sleeps\n",
            short_emul_delta, short_real_delta, short_sleeps);
    CHECK(short_emul_delta >= 16000 && short_emul_delta <= 18000);
    /* emulated time = max(real, deadline): the carried remainder is < 1 ms */
    CHECK(short_emul_delta >= short_real_delta);
    CHECK(short_emul_delta - short_real_delta < 1000);
    CHECK(short_min_ok);
    CHECK(short_sleeps < 30);

    /* computing between waits is not forgiven, short waits still advance */
    phase = 3;
    fake_now = 3000000;
    host_configure(&real);
    CHECK_EQ(host_run_game(), 0);
    CHECK(compute_ok);
    fake_sleep_min_us = 0;

    return CHECK_DONE();
}
