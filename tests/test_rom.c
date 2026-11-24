/* RuntimeInit: ROM function pointers, MemCopy/MemSet and the random
   generator seeded deterministically from host_config.seed. */
/* SDK headers one by one: includes.h (lib_print.h getchar) clashes with stdio.h */
#include <string.h>
#include "_lib/inc/lib_rand.h"
#include "_lib/inc/lib_malloc.h"

#include "check.h"
#include "plat_fake.h"

#define NSEQ 10
static u32 seq[NSEQ];

int host_call_game_main(void)
{
    int i;
    CHECK_EQ(clz(1), 31);
    CHECK_EQ(ctz(8), 3);
    CHECK_EQ(popcount(0xF0F0), 8);
    CHECK_EQ(reverse(1), 0x80000000UL);

    u8 a[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    u8 b[8] = { 0 };
    MemCopy(b, a, 8);
    for (i = 0; i < 8; i++) CHECK_EQ(b[i], i + 1);
    MemSet(b, 0x5A, 4);
    for (i = 0; i < 4; i++) CHECK_EQ(b[i], 0x5A);
    CHECK_EQ(b[4], 5);

    u32 w[4] = { 1, 2, 3, 4 };
    u32 w2[4] = { 0 };
    MemCopy32(w2, w, sizeof(w));
    CHECK_EQ(w2[3], 4);
    MemSet32(w2, 0, sizeof(w2));
    CHECK_EQ(w2[0] | w2[1] | w2[2] | w2[3], 0);

    /* SDK allocator over the 64 KiB host/heap.S region */
    CHECK_EQ(MemGetTotal(), 65536);
    CHECK_EQ((u32)MemGetBase() & 7, 0);
    u8* m = (u8*)MemAlloc(1000);
    CHECK(m != NULL);
    CHECK(m >= (u8*)MemGetBase() && m + 1000 <= (u8*)MemGetBase() + 65536);
    MemFree(m);

    for (i = 0; i < 100; i++) CHECK(RandU8Max(9) <= 9);

    for (i = 0; i < NSEQ; i++) seq[i] = RandU32();
    return 0;
}

static void run(unsigned int seed, u32* out)
{
    host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = seed, .audio_rate = 0 };
    host_configure(&cfg);
    CHECK_EQ(host_run_game(), 0);
    memcpy(out, seq, sizeof(seq));
}

int main(void)
{
    u32 r1[NSEQ], r2[NSEQ], r3[NSEQ];
    int i, same12 = 1, same13 = 1;
    run(1, r1);
    run(1, r2);
    run(2, r3);
    for (i = 0; i < NSEQ; i++) {
        if (r1[i] != r2[i]) same12 = 0;
        if (r1[i] != r3[i]) same13 = 0;
    }
    CHECK(same12); /* same seed -> same sequence */
    CHECK(!same13); /* different seed -> different sequence */
    return CHECK_DONE();
}
