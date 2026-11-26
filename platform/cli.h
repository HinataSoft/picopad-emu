/* Command line of a game .exe. Pure C: no SDL, no SDK headers. */
#ifndef PICOPAD_PLATFORM_CLI_H
#define PICOPAD_PLATFORM_CLI_H

#include "keymap.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int scale;                 /* window scale, 1..8 (default 3) */
    keymap_mode keys;          /* default KEYMAP_LETTERS */
    int fast_display;          /* 1 = no SPI transfer cost (default 0) */
    unsigned int seed;         /* RoscRand seed (default from time) */
    int seed_set;              /* 1 = --seed was given */
    long long max_time_ms;     /* 0 = no limit; exit code 3 when reached (headless default 60000) */
    int max_time_set;          /* 1 = --max-time was given */
    int headless;              /* 1 = --headless: no window, virtual time */
    const char* script;        /* --script file (points into argv), NULL = none */
    const char* shot_dir;      /* --shot-dir (points into argv), default "." */
    const char* wav;           /* --wav file (points into argv), NULL = none; needs headless */
    int trace_keys;            /* 1 = --trace-keys */
    const char* sd;            /* --sd image file (points into argv), NULL = no card */
} cli_options;

/* return values of cli_parse */
#define CLI_OK    0
#define CLI_ERROR 1            /* message in err; the exe exits with code 2 */
#define CLI_HELP  2            /* --help / -h: usage text in err; exit code 0 */

/* Options (each also as --opt=value):
     --scale N   --keys letters|layout   --fast-display   --seed N
     --max-time T   (T = 2000, 2000ms or 2s)
     --headless   --script FILE   --shot-dir DIR   --wav FILE   --trace-keys
     --sd FILE   (SD card image; valid with and without --headless)
   Headless defaults (resolved after parsing, option order does not matter):
   max_time_ms 60000 without --max-time, seed 0 without --seed (seed_set stays 0).
   --wav, --script and --shot-dir require --headless; --scale and --keys are accepted and ignored there.
   err receives a NUL-terminated message (at most errlen bytes). */
int cli_parse(int argc, char** argv, cli_options* out, char* err, int errlen);

/* usage text (multi-line, without trailing newline) */
const char* cli_usage(void);

#ifdef __cplusplus
}
#endif
#endif
