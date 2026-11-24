/* Clock and SysTick pump: virtual clock (WaitMs/WaitUs, SysTime every 5 ms)
   and the busy-wait protection of the real clock (test_busy_wait). */
/* SDK headers one by one: includes.h (lib_print.h getchar) clashes with stdio.h */
#include "_sdk/inc/sdk_systick.h"  /* SysTime */
#include "_sdk/inc/sdk_watchdog.h" /* ResetToBootLoader */
#include "_lib/inc/lib_alarm.h"    /* sAlarm: observer called from the SysTick handler */

#include "check.h"
#include "plat_fake.h"

static int phase; /* 0 = virtual clock test, 1 = busy-wait test */
static unsigned long long busy_delta;

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

int host_call_game_main(void)
{
    if (phase == 0)
        virtual_clock_game();
    else
        busy_wait_game();
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

    return CHECK_DONE();
}
