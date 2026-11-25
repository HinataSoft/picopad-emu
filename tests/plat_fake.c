/* Fake platform for unit tests: implementation of include/plat_api.h. */
#include <stdio.h>
#include <string.h>

#include "plat_fake.h"

unsigned long long fake_now = 0;
unsigned long long fake_sleep_min_us = 0;
int fake_sleep_count = 0;
unsigned short fake_fb[PLAT_H][PLAT_W];
int fake_present_count = 0;
int fake_present_x = 0, fake_present_y = 0, fake_present_w = 0, fake_present_h = 0;
unsigned int fake_keys = 0;
char fake_log[256];
int fake_log_count = 0;
short fake_audio[65536];
int fake_audio_count = 0;

unsigned long long plat_now_us(void) { return fake_now; }

void plat_sleep_us(unsigned long long us)
{
    fake_sleep_count++;
    fake_now += (us < fake_sleep_min_us) ? fake_sleep_min_us : us;
}

void plat_present(const unsigned short* fb, int x, int y, int w, int h)
{
    int i;
    fake_present_count++;
    fake_present_x = x;
    fake_present_y = y;
    fake_present_w = w;
    fake_present_h = h;
    for (i = 0; i < h; i++)
        memcpy(&fake_fb[y + i][x], &fb[(y + i) * PLAT_W + x], (size_t)w * sizeof(unsigned short));
}

unsigned int plat_key_bits(void) { return fake_keys; }

void plat_log(const char* msg)
{
    fake_log_count++;
    snprintf(fake_log, sizeof(fake_log), "%s", msg);
    fprintf(stderr, "plat_log: %s\n", msg);
}

void plat_audio_write(const short* s, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (fake_audio_count < (int)(sizeof(fake_audio) / sizeof(fake_audio[0])))
            fake_audio[fake_audio_count] = s[i];
        fake_audio_count++;
    }
}
