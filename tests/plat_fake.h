/* Fake platform for unit tests (tests/plat_fake.c): implements plat_api.h
   with controllable time, keys and a captured display. Plain C types only. */
#ifndef PICOPAD_TESTS_PLAT_FAKE_H
#define PICOPAD_TESTS_PLAT_FAKE_H

#include "../include/plat_api.h"

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned long long fake_now;            /* plat_now_us(); plat_sleep_us adds to it */
extern unsigned long long fake_sleep_min_us;   /* models OS sleep granularity: a sleep
                                                  advances at least this much (0 = exact) */
extern int fake_sleep_count;                   /* number of plat_sleep_us() calls */
extern unsigned short fake_fb[PLAT_H][PLAT_W]; /* copy of every presented rectangle */
extern int fake_present_count;                 /* number of plat_present() calls */
extern int fake_present_x, fake_present_y, fake_present_w, fake_present_h; /* last call */
extern unsigned int fake_keys;                 /* returned by plat_key_bits() */
extern char fake_log[256];                     /* last plat_log() message */
extern int fake_log_count;                     /* number of plat_log() calls */
extern short fake_audio[65536];                /* first 65536 samples passed to plat_audio_write */
extern int fake_audio_count;                   /* all samples passed to plat_audio_write */

#ifdef __cplusplus
}
#endif
#endif
