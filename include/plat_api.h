/* host <-> platform boundary. Plain C types only: no <stdint.h>, no SDK headers. */
#ifndef PLAT_API_H
#define PLAT_API_H
#ifdef __cplusplus
extern "C" {
#endif

#define PLAT_W 320
#define PLAT_H 240
/* bit i of plat_key_bits() = Picopad key code i+1: UP=0, LEFT=1, RIGHT=2, DOWN=3, X=4, Y=5, A=6, B=7 */
#define PLAT_KEY_BIT(code) (1u << ((code) - 1))

#define PLAT_AUDIO_RING    1   /* feed the ring drained by host_audio_render (audio device) */
#define PLAT_AUDIO_CAPTURE 2   /* pass every sample to plat_audio_write */

typedef struct {
    int virtual_clock;          /* 0 = real time, 1 = virtual time */
    int fast_display;           /* 1 = no SPI transfer cost */
    unsigned int seed;          /* RoscRand seed */
    int audio_rate;             /* mixer rate Hz; 0 = 44100 */
    int audio_out;              /* PLAT_AUDIO_* bits; 0 = mixer runs, samples discarded */
} host_config;

/* implemented by platform/ (or tests/plat_fake.c), called by host/ */
unsigned long long plat_now_us(void);                 /* monotonic real time */
void plat_sleep_us(unsigned long long us);
void plat_present(const unsigned short* fb, int x, int y, int w, int h); /* copy rect of the 320-wide RGB565 buffer */
unsigned int plat_key_bits(void);
void plat_log(const char* msg);
void plat_audio_write(const short* s, int n);        /* PLAT_AUDIO_CAPTURE samples, in order */
int  plat_tick(unsigned long long t_us);             /* before each SysTick; <0 = go on, >=0 = end game with this exit code */

/* implemented by host/, called by platform/ */
void host_configure(const host_config* cfg);
int  host_run_game(void);                 /* RuntimeInit + game_main on the calling thread; returns exit code */
void host_audio_render(short* out, int frames);   /* mono s16; called from audio thread */
unsigned long long host_now_us(void);     /* emulated device time (µs since start) */
void host_watchdog(void);                 /* from one non-game thread every ~20 ms: unsticks a game
                                             looping without SDK calls (see host/pump.c) */
unsigned int host_stall_count(void);      /* watchdog interventions since host_run_game start */
int  host_pending_exit(void);             /* exit code requested off the game thread (watchdog: plat_tick
                                             end code, or 1 after a fatal error in a watchdog step)
                                             and not yet honoured by a game pump; -1 = none */

#ifdef __cplusplus
}
#endif
#endif
