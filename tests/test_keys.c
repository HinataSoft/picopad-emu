/* Keys via the GPIO shim: KeyScan from the emulated SysTick reads host_gpio_in. */
#include "_sdk/inc/sdk_systick.h"
#include "_sdk/inc/sdk_watchdog.h"
#include "_devices/picopad/picopad_key.h"

#include "check.h"
#include "plat_fake.h"

static void keys_game(void)
{
    u8 k1, k2;
    fake_keys = PLAT_KEY_BIT(KEY_A) | PLAT_KEY_BIT(KEY_LEFT);
    WaitMs(5);
    CHECK(KeyPressed(KEY_A));
    CHECK(KeyPressed(KEY_LEFT));
    CHECK(!KeyPressed(KEY_B));
    k1 = KeyGet();
    k2 = KeyGet();
    CHECK((k1 == KEY_A && k2 == KEY_LEFT) || (k1 == KEY_LEFT && k2 == KEY_A));
    fake_keys = 0;
    WaitMs(100);
    CHECK(!KeyPressed(KEY_A));
    CHECK(!KeyPressed(KEY_LEFT));

    /* every key individually */
    {
        int code;
        for (code = KEY_UP; code <= KEY_B; code++) {
            int c2;
            fake_keys = PLAT_KEY_BIT(code);
            WaitMs(60);
            for (c2 = KEY_UP; c2 <= KEY_B; c2++) CHECK_EQ(KeyPressed(c2) ? 1 : 0, c2 == code);
            fake_keys = 0;
            WaitMs(60);
        }
    }
    ResetToBootLoader();
}

int host_call_game_main(void)
{
    keys_game();
    return 0;
}

int main(void)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 0 };
    host_configure(&cfg);
    CHECK_EQ(host_run_game(), 0);
    return CHECK_DONE();
}
