/* Headless run of a game .exe (--headless): virtual time, no window, no
   audio device (SDL_Init is never called), input script, PNG shots, optional
   WAV, one log line per event on stdout.

   Threads:
    - game thread: host_run_game(); plat_present, plat_tick, plat_audio_write
      normally run here;
    - calling (main) thread: host_watchdog() every WATCHDOG_MS until the game
      thread is done. A watchdog step may call plat_tick / plat_audio_write
      too, under the host pump lock, so never concurrently with the game.
   When an end code was returned by plat_tick but the game thread does not
   finish within HANG_KILL_MS of real time (it loops without SDK calls, and
   the watchdog no longer steps once an exit is pending), the end line is
   printed, the WAV closed and the process terminated with that code. The
   same holds for an exit requested by the host without plat_tick (a fatal
   error inside a watchdog step, host_pending_exit): end reason=fatal.

   The 1 ms system timer resolution (timeBeginPeriod) keeps the WATCHDOG_MS
   period close to 20 ms, as in the interactive mode.

   Determinism: virtual time only; real time is used solely by the watchdog
   (stalls=N > 0 in the end line marks a best-effort run) and the hang kill.

   C without SDK headers; from SDL only threads, atomics and time. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <mmsystem.h>
#include <SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../include/plat_api.h"
#include "headless.h"
#include "png.h"
#include "script.h"
#include "wav.h"

#define GAME_STACK_SIZE (4u * 1024 * 1024)
#define WATCHDOG_MS 20          /* host_watchdog period [ms] */
#define HANG_KILL_MS 1000       /* real time allowed for the game thread to end after an end code */
#define AUDIO_RATE 44100

static const cli_options* Opt;
static const char* GameName;

/* script player state: plat_tick only (game thread or watchdog under the pump lock) */
static script Script;
static int HasScript;
static int ScriptDone;          /* quit reached: script_step is not called any more */
static unsigned int TracedKeys; /* last bits printed by --trace-keys */
static int DirReady;            /* shot directory created */
static int ShotError;           /* a shot could not be written */
static unsigned long long ShotT;/* t_us of the tick running script_step */

static SDL_atomic_t Keys;       /* script key bits, read by plat_key_bits */
static unsigned short Disp[PLAT_H * PLAT_W]; /* display content as seen by plat_present (game thread) */

static wav_writer* Wav;
static volatile int WavError;   /* wav_write failed (set by plat_audio_write) */

/* end requested by plat_tick; the fields are written before EndSet (full barrier) */
static SDL_atomic_t EndSet;
static int EndCode;
static const char* EndReason;
static unsigned long long EndUs;

static SDL_atomic_t GameDone;
static SDL_atomic_t GameCode;

/* one stdout log line, flushed at once */
static void log_line(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fputc('\n', stdout);
    fflush(stdout);
}

static int is_sep(char c)
{
    return c == '/' || c == '\\';
}

/* creates dir with all parents (both / and \ accepted); 0 = it is a directory */
static int make_dirs(const char* dir)
{
    char buf[MAX_PATH];
    size_t n = strlen(dir), i;
    DWORD attr;
    if (n == 0) return 0;               /* current directory */
    if (n >= sizeof(buf)) return -1;
    memcpy(buf, dir, n + 1);
    for (i = 1; i < n; i++)
    {
        if (!is_sep(buf[i]) || is_sep(buf[i - 1]) || buf[i - 1] == ':') continue;
        buf[i] = '\0';
        CreateDirectoryA(buf, NULL);    /* errors (exists, root, share) decided by the final check */
        buf[i] = dir[i];
    }
    CreateDirectoryA(buf, NULL);
    attr = GetFileAttributesA(buf);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) ? 0 : -1;
}

/* script_sink.shot: <shot_dir>/<name>.png from the display buffer */
static void shot(void* ctx, const char* name)
{
    char path[MAX_PATH + 80];
    const char* dir = Opt->shot_dir ? Opt->shot_dir : ".";
    size_t n = strlen(dir);
    (void)ctx;
    if (ShotError) return;
    if (!DirReady)
    {
        if (make_dirs(dir) != 0)
        {
            fprintf(stderr, "%s: cannot create shot directory '%s'\n", GameName, dir);
            fflush(stderr);
            ShotError = 1;
            return;
        }
        DirReady = 1;
    }
    if (n == 0 || is_sep(dir[n - 1]))
        snprintf(path, sizeof(path), "%s%s.png", dir, name);
    else
        snprintf(path, sizeof(path), "%s/%s.png", dir, name);
    if (png_write_rgb565(path, Disp, PLAT_W, PLAT_H, PLAT_W) != 0)
    {
        fprintf(stderr, "%s: cannot write '%s'\n", GameName, path);
        fflush(stderr);
        ShotError = 1;
        return;
    }
    log_line("shot t=%llu file=%s", ShotT / 1000, path);
}

static int request_end(int code, const char* reason, unsigned long long t_us)
{
    EndCode = code;
    EndReason = reason;
    EndUs = t_us;
    SDL_AtomicSet(&EndSet, 1);
    return code;
}

/* ---------------------------------------------------------------------------
   plat_* implementations (main.cpp delegates here while headless) */

int headless_tick(unsigned long long t_us)
{
    int quit = 0;
    if (SDL_AtomicGet(&EndSet)) return EndCode;
    if (WavError) return request_end(1, "fatal", t_us);

    if (HasScript && !ScriptDone)
    {
        script_sink sink;
        sink.shot = shot;
        sink.ctx = NULL;
        ShotT = t_us;
        quit = script_step(&Script, t_us, &sink);
        if (quit) ScriptDone = 1;
        SDL_AtomicSet(&Keys, (int)Script.keys);
    }
    if (Opt->trace_keys)
    {
        unsigned int bits = (unsigned int)SDL_AtomicGet(&Keys);
        if (bits != TracedKeys)
        {
            TracedKeys = bits;
            log_line("keys t=%llu bits=0x%02x", t_us / 1000, bits);
        }
    }
    if (ShotError) return request_end(1, "fatal", t_us);
    if (quit) return request_end(0, "quit", t_us);
    if (Opt->max_time_ms > 0 && t_us >= (unsigned long long)Opt->max_time_ms * 1000ull)
        return request_end(3, "max-time", t_us);
    return -1;
}

void headless_audio_write(const short* s, int n)
{
    if (Wav == NULL || WavError) return;
    if (wav_write(Wav, s, n) != 0)
    {
        fprintf(stderr, "%s: cannot write '%s'\n", GameName, Opt->wav);
        fflush(stderr);
        WavError = 1;
    }
}

unsigned int headless_key_bits(void)
{
    return (unsigned int)SDL_AtomicGet(&Keys);
}

void headless_present(const unsigned short* fb, int x, int y, int w, int h)
{
    int row;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > PLAT_W) w = PLAT_W - x;
    if (y + h > PLAT_H) h = PLAT_H - y;
    if (w <= 0 || h <= 0) return;
    for (row = y; row < y + h; row++)
        memcpy(&Disp[row * PLAT_W + x], &fb[row * PLAT_W + x], (size_t)w * sizeof(unsigned short));
}

/* --------------------------------------------------------------------------- */

static int SDLCALL game_thread(void* arg)
{
    int code;
    (void)arg;
    code = host_run_game();
    SDL_AtomicSet(&GameCode, code);
    SDL_AtomicSet(&GameDone, 1);
    return code;
}

/* closes the WAV, prints the end line; returns the final exit code */
static int finish(const char* reason, unsigned long long t_ms, int code)
{
    if (Wav != NULL)
    {
        if (wav_close(Wav) != 0 && !WavError)
        {
            fprintf(stderr, "%s: cannot write '%s'\n", GameName, Opt->wav);
            fflush(stderr);
            WavError = 1;
        }
        Wav = NULL;
    }
    if (WavError)
    {
        reason = "fatal";
        code = 1;
    }
    log_line("end reason=%s t=%llu code=%d stalls=%u pending=%d", reason, t_ms, code,
        host_stall_count(), HasScript ? script_pending(&Script) : 0);
    fflush(stderr);
    return code;
}

int headless_run(const cli_options* opt, const char* game_name)
{
    char err[512];
    host_config cfg;
    SDL_Thread* thread;
    Uint32 end_seen = 0;
    int end_seen_set = 0, code;

    Opt = opt;
    GameName = game_name;
    if (opt->script)
    {
        if (script_load(opt->script, &Script, err, (int)sizeof(err)) != 0)
        {
            fprintf(stderr, "%s: %s: %s\n", game_name, opt->script, err);
            return 2;
        }
        HasScript = 1;
    }

    log_line("start game=%s seed=%u rate=%d", game_name, opt->seed, AUDIO_RATE);

    if (opt->wav)
    {
        Wav = wav_open(opt->wav, AUDIO_RATE);
        if (Wav == NULL)
        {
            fprintf(stderr, "%s: cannot write '%s'\n", game_name, opt->wav);
            return finish("fatal", 0, 1);
        }
    }

    cfg.virtual_clock = 1;
    cfg.fast_display = opt->fast_display;
    cfg.seed = opt->seed;
    cfg.audio_rate = AUDIO_RATE;
    cfg.audio_out = opt->wav ? PLAT_AUDIO_CAPTURE : 0;
    host_configure(&cfg);

    timeBeginPeriod(1);   /* released by timeEndPeriod below, or by the OS on TerminateProcess */
    thread = SDL_CreateThreadWithStackSize(game_thread, "game", GAME_STACK_SIZE, NULL);
    if (thread == NULL)
    {
        fprintf(stderr, "%s: cannot create game thread: %s\n", game_name, SDL_GetError());
        timeEndPeriod(1);
        return finish("fatal", 0, 1);
    }

    while (!SDL_AtomicGet(&GameDone))
    {
        host_watchdog();   /* no-op unless the game loops without SDK calls */
        if ((SDL_AtomicGet(&EndSet) || host_pending_exit() >= 0) && !SDL_AtomicGet(&GameDone))
        {
            if (!end_seen_set)
            {
                end_seen_set = 1;
                end_seen = SDL_GetTicks();
            }
            else if (SDL_GetTicks() - end_seen >= HANG_KILL_MS)
            {
                /* the game thread hangs in a loop without SDK calls */
                if (SDL_AtomicGet(&EndSet))
                    code = finish(EndReason, EndUs / 1000, EndCode);
                else   /* fatal error in a watchdog step (host_pending_exit() == 1) */
                    code = finish("fatal", host_now_us() / 1000, host_pending_exit());
                fflush(stdout);
                fflush(stderr);
                TerminateProcess(GetCurrentProcess(), (UINT)code);
                ExitProcess((UINT)code);   /* not reached */
            }
        }
        SDL_Delay(WATCHDOG_MS);
    }
    SDL_WaitThread(thread, NULL);
    timeEndPeriod(1);

    code = SDL_AtomicGet(&GameCode);
    if (SDL_AtomicGet(&EndSet) && code == EndCode)
        code = finish(EndReason, EndUs / 1000, code);
    else
        code = finish(code == 0 ? "exit" : "fatal", host_now_us() / 1000, code);
    script_free(&Script);
    return code;
}
