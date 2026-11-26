/* host/sd.c: the lib_sd API over plat_sd_* (no card, card, read-only card,
   out of range). Uses the fake platform's in-memory card. */
#include <string.h>
#include "_lib/inc/lib_sd.h"

#include "check.h"
#include "plat_fake.h"

#define SECTORS 200u

int host_call_game_main(void) { return 0; }   /* never run: only the SD API is tested */

int main(void)
{
    static unsigned char card[SECTORS * 512];
    unsigned char buf[512], back[512];
    unsigned int i;

    /* no card: connect fails, size 0, type none, I/O fails */
    CHECK(!SDConnect());
    CHECK_EQ(SDType, SD_NONE);
    CHECK(!DiskValid());
    CHECK_EQ(SDMediaSize(), 0);
    CHECK(!SDReadSect(0, buf));
    CHECK(!SDWriteSect(0, buf));

    /* card present */
    for (i = 0; i < sizeof(card); i++) card[i] = (unsigned char)(i * 7 + (i >> 9));
    fake_sd_data = card;
    fake_sd_sectors = SECTORS;
    CHECK(SDConnect());
    CHECK_EQ(SDType, SD_SDHC);
    CHECK(DiskValid());
    CHECK_EQ(SDMediaSize(), SECTORS);

    CHECK(SDReadSect(3, buf));
    CHECK(memcmp(buf, card + 3 * 512, 512) == 0);
    CHECK(SDReadSect(SECTORS - 1, buf));
    CHECK(memcmp(buf, card + (SECTORS - 1) * 512, 512) == 0);
    CHECK(!SDReadSect(SECTORS, buf));        /* out of range */

    memset(buf, 0xA5, sizeof(buf));
    CHECK(SDWriteSect(5, buf));
    CHECK(SDReadSect(5, back));
    CHECK(memcmp(buf, back, 512) == 0);
    CHECK(memcmp(card + 5 * 512, buf, 512) == 0);
    CHECK_EQ(fake_sd_writes, 1);
    CHECK(!SDWriteSect(SECTORS, buf));       /* out of range */
    CHECK_EQ(fake_sd_writes, 1);

    /* read-only card: reads work, writes fail */
    fake_sd_readonly = 1;
    CHECK(SDReadSect(5, back));
    CHECK(!SDWriteSect(6, buf));
    CHECK_EQ(fake_sd_writes, 1);
    fake_sd_readonly = 0;

    /* disconnect invalidates the connection until SDConnect again */
    SDDisconnect();
    CHECK_EQ(SDType, SD_NONE);
    CHECK(!DiskValid());
    CHECK(SDConnect());
    CHECK_EQ(SDType, SD_SDHC);

    /* init/term do not touch the card */
    SDInit();
    SDTerm();
    CHECK(SDReadSect(5, back));
    return CHECK_DONE();
}
