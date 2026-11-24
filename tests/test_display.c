/* Display: dirty rect -> plat_present, SPI transfer time model (w*h*16/24 us). */
/* SDK headers one by one: includes.h (lib_print.h getchar) clashes with stdio.h */
#include "_sdk/inc/sdk_systick.h"
#include "_sdk/inc/sdk_watchdog.h"
#include "_lib/inc/lib_draw.h"
#include "_display/st7789/st7789.h"

#include "check.h"
#include "plat_fake.h"

static int phase; /* 0 = SPI transfer model, 1 = fast display */

static void model_game(void)
{
    u32 t0;
    int cnt;
    DrawClear(); /* marks full screen dirty */
    DrawRect(10, 20, 30, 40, COL_RED);
    t0 = Time();
    DispUpdate();
    CHECK_EQ(fake_present_w, 320);
    CHECK_EQ(fake_present_h, 240);
    CHECK_EQ(fake_fb[20][10], COL_RED);
    CHECK_EQ(fake_fb[59][39], COL_RED);
    CHECK_EQ(fake_fb[60][40], COL_BLACK);
    CHECK_EQ(Time() - t0, 51200); /* full-screen SPI transfer at 24 MHz */

    DrawPoint(319, 239, COL_WHITE); /* edge rect */
    t0 = Time();
    DispUpdate();
    CHECK_EQ(fake_present_x, 319);
    CHECK_EQ(fake_present_y, 239);
    CHECK_EQ(fake_present_w, 1);
    CHECK_EQ(fake_present_h, 1);
    CHECK_EQ(fake_fb[239][319], COL_WHITE);
    CHECK_EQ(Time() - t0, 1 * 1 * 16 / 24); /* 0 us after integer division */

    DrawRect(300, 200, 20, 40, COL_GREEN); /* touches both right and bottom edge */
    t0 = Time();
    DispUpdate();
    CHECK_EQ(fake_present_x, 300);
    CHECK_EQ(fake_present_y, 200);
    CHECK_EQ(fake_present_w, 20);
    CHECK_EQ(fake_present_h, 40);
    CHECK_EQ(fake_fb[239][319], COL_GREEN);
    CHECK_EQ(Time() - t0, 20 * 40 * 16 / 24);

    cnt = fake_present_count; /* nothing dirty -> no present, no time */
    t0 = Time();
    DispUpdate();
    CHECK_EQ(fake_present_count, cnt);
    CHECK_EQ(Time() - t0, 0);
}

static void fast_game(void)
{
    u32 t0;
    DrawClear();
    t0 = Time();
    DispUpdate();
    CHECK_EQ(fake_present_w, 320);
    CHECK_EQ(fake_present_h, 240);
    CHECK_EQ(Time() - t0, 0); /* no transfer time with --fast-display */
}

int host_call_game_main(void)
{
    if (phase == 0)
        model_game();
    else
        fast_game();
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 0, .seed = 1, .audio_rate = 0 };
    phase = 0;
    host_configure(&cfg);
    CHECK_EQ(host_run_game(), 0);

    cfg.fast_display = 1;
    phase = 1;
    host_configure(&cfg);
    CHECK_EQ(host_run_game(), 0);
    return CHECK_DONE();
}
