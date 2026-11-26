/* Fake platform for unit tests: implementation of include/plat_api.h. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "plat_fake.h"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

volatile unsigned long long fake_now = 0; /* shared with test helper threads: atomic access */
unsigned long long fake_sleep_min_us = 0;
int fake_sleep_count = 0;
int fake_real_time = 0;
unsigned short fake_fb[PLAT_H][PLAT_W];
int fake_present_count = 0;
int fake_present_x = 0, fake_present_y = 0, fake_present_w = 0, fake_present_h = 0;
unsigned int fake_keys = 0;
char fake_log[256];
int fake_log_count = 0;
short fake_audio[65536];
int fake_audio_count = 0;
unsigned long long fake_tick_times[64];
int fake_tick_count = 0;
int (*fake_tick_fn)(unsigned long long t_us) = NULL;

/* real clock and sleep of the window platform (copy of platform/main.cpp:
   QPC microseconds, high resolution waitable timer, else Sleep) */
static unsigned long long real_now_us(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (unsigned long long)(c.QuadPart / f.QuadPart) * 1000000ull +
           (unsigned long long)(c.QuadPart % f.QuadPart) * 1000000ull / (unsigned long long)f.QuadPart;
}

static void real_sleep_us(unsigned long long us)
{
    static HANDLE timer = NULL;
    static int timer_tried = 0;
    if (us == 0) { Sleep(0); return; }
    if (!timer_tried) {
        timer_tried = 1;
        timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    }
    if (timer != NULL) {
        LARGE_INTEGER due;
        due.QuadPart = -(LONGLONG)(us * 10);
        if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
    Sleep((DWORD)((us + 999) / 1000));
}

unsigned long long plat_now_us(void)
{
    if (fake_real_time) return real_now_us();
    return __atomic_load_n(&fake_now, __ATOMIC_SEQ_CST);
}

void plat_sleep_us(unsigned long long us)
{
    fake_sleep_count++;
    if (fake_real_time) { real_sleep_us(us); return; }
    __atomic_add_fetch(&fake_now, (us < fake_sleep_min_us) ? fake_sleep_min_us : us, __ATOMIC_SEQ_CST);
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

int plat_tick(unsigned long long t_us)
{
    if (fake_tick_count < 64) fake_tick_times[fake_tick_count] = t_us;
    fake_tick_count++;
    return fake_tick_fn ? fake_tick_fn(t_us) : -1;
}

unsigned char* fake_sd_data = NULL;
unsigned int fake_sd_sectors = 0;
int fake_sd_readonly = 0;
int fake_sd_writes = 0;

unsigned int plat_sd_sectors(void) { return fake_sd_data ? fake_sd_sectors : 0; }

int plat_sd_read(unsigned int sector, unsigned char* buf512)
{
    if (!fake_sd_data || sector >= fake_sd_sectors) return 0;
    memcpy(buf512, fake_sd_data + (size_t)sector * 512, 512);
    return 1;
}

int plat_sd_write(unsigned int sector, const unsigned char* buf512)
{
    if (!fake_sd_data || fake_sd_readonly || sector >= fake_sd_sectors) return 0;
    memcpy(fake_sd_data + (size_t)sector * 512, buf512, 512);
    fake_sd_writes++;
    return 1;
}
