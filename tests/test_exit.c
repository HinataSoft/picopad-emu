/* Exit-code contract of host_run_game: panic() and FatalErrorStop() end the
   game with exit code 1 and log the message through plat_log; nothing after
   the call runs. */
/* SDK headers one by one: includes.h (lib_print.h getchar) clashes with stdio.h */
#include <string.h>
#include "_sdk/inc/sdk_watchdog.h" /* ResetToBootLoader */

#include "check.h"
#include "plat_fake.h"

static int phase;   /* 0 = panic, 1 = FatalErrorStop, 2 = ResetToBootLoader */
static int reached; /* set by code after the exit call (must stay 0) */

int host_call_game_main(void)
{
    if (phase == 0)
        panic("bad level %d", 42);
    else if (phase == 1)
        FatalErrorStop();
    else
        ResetToBootLoader();
    reached = 1;
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 0 };
    int logs;
    host_configure(&cfg);

    phase = 0;
    logs = fake_log_count;
    CHECK_EQ(host_run_game(), 1);
    CHECK_EQ(fake_log_count, logs + 1);
    CHECK(strstr(fake_log, "bad level 42") != NULL);

    phase = 1;
    logs = fake_log_count;
    fake_log[0] = 0;
    CHECK_EQ(host_run_game(), 1);
    CHECK_EQ(fake_log_count, logs + 1);
    CHECK(strstr(fake_log, "FatalErrorStop") != NULL);

    /* contrast: ResetToBootLoader is a normal exit (0) and logs nothing */
    phase = 2;
    logs = fake_log_count;
    CHECK_EQ(host_run_game(), 0);
    CHECK_EQ(fake_log_count, logs);

    CHECK_EQ(reached, 0);
    return CHECK_DONE();
}
