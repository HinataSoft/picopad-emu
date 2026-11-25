/* Stall protection: in virtual time, PUMP_IDLE_CALLS pumps without time
   progress advance time by one tick (deterministic); a game loop without any
   SDK call (while (PlayingSound()) {}) is unstuck by host_watchdog(), called
   from another thread. */
#include <string.h>

#include "_sdk/inc/sdk_watchdog.h"
#include "_lib/inc/lib_pwmsnd.h"

#include "check.h"
#include "plat_fake.h"

void* thr_start(void (*fn)(void*), void* arg);
int thr_join(void* t, int timeout_ms);
unsigned long long thr_ms(void);

static u8 snd[4410]; /* 0.2 s @22050 Hz */

static int run_mode;                /* 0 = stall + watchdog, 1 = idle advance (no watchdog),
                                       2 = stall on the real clock, 3 = exit request from the watchdog */
static volatile int stall_loop_done;
static volatile int helper_stop;
static volatile int release_game;    /* mode 3: lets the stalled loop end */
static volatile int exit_returned;   /* mode 3: plat_tick returned the exit code */
static unsigned int stalls_at_exit = 98, stalls_after = 99;
static void* helper;
static unsigned int idle_stalls = 99;
static u32 dmb_delta, idle_t0;

/* helper thread: fake real time runs 20 ms per call, watchdog called each time */
static void watchdog_thread(void* arg)
{
    unsigned long long start = thr_ms();
    (void)arg;
    while (!stall_loop_done && !helper_stop && thr_ms() - start < 5000) {
        __atomic_add_fetch(&fake_now, 20000, __ATOMIC_SEQ_CST);
        host_watchdog();
        if (run_mode == 3 && exit_returned) {
            /* exit pending: further watchdog calls must not step the game */
            int i;
            stalls_at_exit = host_stall_count();
            for (i = 0; i < 20; i++) {
                __atomic_add_fetch(&fake_now, 20000, __ATOMIC_SEQ_CST);
                host_watchdog();
            }
            stalls_after = host_stall_count();
            release_game = 1;
            break;
        }
    }
}

static int exit_tick_fn(unsigned long long t)
{
    if (t != 100000ULL) return -1;
    exit_returned = 1;
    return 9;
}

static void stall_game(void)
{
    int i;
    for (i = 0; i < (int)sizeof(snd); i++) snd[i] = 0xC0;
    PlaySound(snd, sizeof(snd));                 /* 0.2 s */
    helper = thr_start(watchdog_thread, 0);
    while (PlayingSound() && !release_game) {}   /* no SDK call that pumps */
    stall_loop_done = 1;
}

static void idle_game(void)
{
    int i;
    u32 t0;
    WaitMs(5);
    t0 = Time();
    idle_t0 = t0;
    while (Time() - t0 < 50000) {}              /* polling Time() alone advances virtual time */
    idle_stalls = host_stall_count();
    WaitMs(5);                                   /* idle counter starts at 0 */
    t0 = Time();
    for (i = 0; i < 5000; i++) dmb();            /* 5 x 1000 pumps -> 5 ticks */
    dmb_delta = Time() - t0;
}

int host_call_game_main(void)
{
    if (run_mode == 1) { idle_game(); return 0; }
    stall_game();
    if (run_mode == 3) {
        WaitMs(1);                               /* honours the exit requested by the watchdog */
        CHECK(0);                                /* not reached */
    }
    return 0;
}

static void run_stall(int mode, int virtual_clock, int expect_code)
{
    host_config cfg = { .virtual_clock = virtual_clock, .fast_display = 1, .seed = 1,
                        .audio_rate = 0, .audio_out = 0 };
    host_configure(&cfg);
    run_mode = mode;
    stall_loop_done = 0;
    helper_stop = 0;
    release_game = 0;
    exit_returned = 0;
    helper = 0;
    fake_log[0] = 0;
    CHECK_EQ(host_run_game(), expect_code);
    helper_stop = 1;
    CHECK(helper && thr_join(helper, 10000));
    CHECK(stall_loop_done);
    CHECK(host_stall_count() >= 1);
    CHECK(strncmp(fake_log, "stall", 5) == 0);
}

int main(void)
{
    /* virtual clock, watchdog thread */
    run_stall(0, 1, 0);

    /* deterministic part: no watchdog */
    {
        host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 0, .audio_out = 0 };
        host_configure(&cfg);
        run_mode = 1;
        CHECK_EQ(host_run_game(), 0);
        CHECK_EQ(idle_stalls, 0);
        CHECK_EQ(host_stall_count(), 0);
        CHECK_EQ(dmb_delta, 25000);
        CHECK_EQ(idle_t0 % 5000, 0);
    }

    /* real clock: the watchdog runs the ticks and the mixer due by real time */
    run_stall(2, 0, 0);

    /* plat_tick ends the game while the watchdog runs the pump: exit is requested */
    fake_tick_fn = exit_tick_fn;
    run_stall(3, 1, 9);
    fake_tick_fn = 0;
    CHECK(exit_returned);
    CHECK(stalls_at_exit >= 1);
    CHECK_EQ(stalls_after, stalls_at_exit);

    return CHECK_DONE();
}
