// ****************************************************************************
//
//                 Platform layer - SDL2 window, keyboard, sound
//
// ****************************************************************************
// Entry point of a game .exe (built by picopad_add_game). Implements the
// plat_* side of include/plat_api.h on top of SDL2 and Win32.
//
// Threads:
//  - main thread: owns SDL (window, renderer, texture, events) and presents the
//    front buffer at display refresh (vsync, or ~60 Hz without vsync);
//  - game thread: host_run_game() = RuntimeInit + the game's main();
//    plat_present() copies the rect into the mutex protected front buffer;
//  - SDL audio thread: the callback only calls host_audio_render();
//  - watchdog thread: host_watchdog() every WATCHDOG_MS until the game is done
//    (unsticks a game looping without SDK calls, see host/pump.c).
//
// Exit codes: game thread finished -> its code (0 ResetToBootLoader, 1 fatal),
// window close / Ctrl+C -> 0, --max-time reached -> 3, bad arguments -> 2.
// A game never returns by itself, so the process is terminated from the main
// thread (TerminateProcess) while the game thread may still be running.
//
// --headless: main() hands over to headless_run (platform/headless.c: no
// window, no audio device, no SDL_Init, virtual time, script and log) and the
// plat_* functions below delegate to its headless_* versions.
//
// This TU includes SDL and windows.h, so it must not include SDK headers.

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
#include "cli.h"
#include "headless.h"
#include "keymap.h"

#ifndef PICOPAD_GAME_NAME
#define PICOPAD_GAME_NAME "picopad"
#endif

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#define GAME_STACK_SIZE (4u * 1024 * 1024)
#define AUDIO_RATE_WANTED 44100
#define AUDIO_SAMPLES 512                  // device buffer [frames]
#define EVENT_WAIT_MS 5                    // main loop wake-up period
#define NOVSYNC_FRAME_US 16000ull          // present interval without vsync
#define WATCHDOG_MS 20                     // host_watchdog period
#define PENDING_KILL_US 1000000ull         // real time for the game to honour a pending exit

// ---------------------------------------------------------------------------
// shared state

static SDL_mutex* FrontMutex;              // guards Front and FrontDirty
static unsigned short Front[PLAT_H * PLAT_W];
static int FrontDirty;

static SDL_atomic_t KeyBits;               // effective (latched) key bits
static SDL_atomic_t GameDone;              // 1 = host_run_game returned
static SDL_atomic_t GameCode;              // its return code

static int Headless;                       // 1 = --headless (plat_* delegate to headless.c)
static int TraceKeys;                      // --trace-keys (interactive: keys lines on stderr)

static SDL_mutex* LogMutex;
static SDL_AudioDeviceID AudioDev;
static Uint32 GameDoneEvent = (Uint32)-1;  // SDL user event that wakes the main loop

// ---------------------------------------------------------------------------
// plat_api.h: implemented here, called by host/

extern "C" unsigned long long plat_now_us(void)
{
	static Uint64 freq = 0;
	if (freq == 0) freq = SDL_GetPerformanceFrequency();
	Uint64 c = SDL_GetPerformanceCounter();
	return (c / freq) * 1000000ull + (c % freq) * 1000000ull / freq;
}

extern "C" void plat_sleep_us(unsigned long long us)
{
	// high resolution waitable timer (Windows 10 1803+), else Sleep with the
	// 1 ms system timer resolution set by timeBeginPeriod(1) in main()
	static thread_local HANDLE timer = NULL;
	static thread_local int timer_tried = 0;
	if (us == 0) { Sleep(0); return; }
	if (!timer_tried)
	{
		timer_tried = 1;
		timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	}
	if (timer != NULL)
	{
		LARGE_INTEGER due;
		due.QuadPart = -(LONGLONG)(us * 10);   // relative, 100 ns units
		if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
		{
			WaitForSingleObject(timer, INFINITE);
			return;
		}
	}
	Sleep((DWORD)((us + 999) / 1000));
}

extern "C" void plat_present(const unsigned short* fb, int x, int y, int w, int h)
{
	if (Headless) { headless_present(fb, x, y, w, h); return; }
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > PLAT_W) w = PLAT_W - x;
	if (y + h > PLAT_H) h = PLAT_H - y;
	if (w <= 0 || h <= 0) return;

	SDL_LockMutex(FrontMutex);
	for (int row = y; row < y + h; row++)
		memcpy(&Front[row * PLAT_W + x], &fb[row * PLAT_W + x], (size_t)w * sizeof(unsigned short));
	FrontDirty = 1;
	SDL_UnlockMutex(FrontMutex);
}

extern "C" unsigned int plat_key_bits(void)
{
	if (Headless) return headless_key_bits();
	return (unsigned int)SDL_AtomicGet(&KeyBits);
}

extern "C" void plat_audio_write(const short* s, int n)
{
	if (Headless) headless_audio_write(s, n);   // PLAT_AUDIO_CAPTURE only in headless
}

static void plat_logf(const char* fmt, ...);

extern "C" int plat_tick(unsigned long long t_us)
{
	if (Headless) return headless_tick(t_us);
	// interactive: the window/Ctrl+C/--max-time end the game; --trace-keys
	// prints the key bits seen by this tick when they change (game thread,
	// or the watchdog thread under the pump lock: never concurrently)
	if (TraceKeys)
	{
		static unsigned int traced = 0;
		unsigned int bits = (unsigned int)SDL_AtomicGet(&KeyBits);
		if (bits != traced)
		{
			traced = bits;
			plat_logf("keys t=%llu bits=0x%02x", t_us / 1000, bits);
		}
	}
	return -1;
}

extern "C" void plat_log(const char* msg)
{
	if (LogMutex) SDL_LockMutex(LogMutex);
	size_t n = strlen(msg);
	fputs(msg, stderr);
	if (n == 0 || msg[n - 1] != '\n') fputc('\n', stderr);
	fflush(stderr);
	if (LogMutex) SDL_UnlockMutex(LogMutex);
}

static void plat_logf(const char* fmt, ...)
{
	char buf[512];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	plat_log(buf);
}

// ---------------------------------------------------------------------------
// keyboard (main thread only)

static keymap_mode Keys;
static keylatch Latch;
// pressed keys indexed by scancode; ScanKey = keycode (label) seen at the
// press, so a release (same scancode) clears exactly what the press set
static unsigned char ScanDown[SDL_NUM_SCANCODES];
static SDL_Keycode ScanKey[SDL_NUM_SCANCODES];

static unsigned int raw_key_bits(void)
{
	unsigned int bits = 0;
	for (int sc = 0; sc < SDL_NUM_SCANCODES; sc++)
		if (ScanDown[sc]) bits |= keymap_bit(Keys, sc, (int)ScanKey[sc]);
	return bits;
}

// called on every key event and once per main loop pass (expires taps)
static void keys_update(void)
{
	unsigned int eff = keylatch_update(&Latch, raw_key_bits(), plat_now_us());
	SDL_AtomicSet(&KeyBits, (int)eff);
}

// ---------------------------------------------------------------------------
// game thread, audio callback

static int SDLCALL game_thread(void* arg)
{
	(void)arg;
	int code = host_run_game();
	SDL_AtomicSet(&GameCode, code);
	SDL_AtomicSet(&GameDone, 1);
	if (GameDoneEvent != (Uint32)-1)
	{
		SDL_Event e;
		SDL_zero(e);
		e.type = GameDoneEvent;
		SDL_PushEvent(&e);
	}
	return code;
}

static int SDLCALL watchdog_thread(void* arg)
{
	(void)arg;
	while (!SDL_AtomicGet(&GameDone))
	{
		host_watchdog();   // no-op unless a game is inside host_run_game
		SDL_Delay(WATCHDOG_MS);
	}
	return 0;
}

static void SDLCALL audio_callback(void* user, Uint8* stream, int len)
{
	(void)user;
	host_audio_render((short*)stream, len / (int)sizeof(short));
}

// Opens the mono s16 device; returns the obtained rate, 0 = no audio
static int audio_open(void)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
	{
		plat_logf("audio: SDL audio init failed (%s), running silent", SDL_GetError());
		return 0;
	}
	SDL_AudioSpec want, have;
	SDL_zero(want);
	want.freq = AUDIO_RATE_WANTED;
	want.format = AUDIO_S16SYS;
	want.channels = 1;
	want.samples = AUDIO_SAMPLES;
	want.callback = audio_callback;
	AudioDev = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (AudioDev == 0)
	{
		plat_logf("audio: cannot open audio device (%s), running silent", SDL_GetError());
		return 0;
	}
	return have.freq;
}

// ---------------------------------------------------------------------------
// exit

static SDL_Thread* GameThread;
static SDL_Thread* WatchdogThread;         // NULL if it could not be created

// Ends the process with 'code'. When the game thread has finished, shuts down
// normally; otherwise terminates the process (the game thread is still
// running a game loop and cannot be stopped).
static void finish(int code)
{
	if (AudioDev != 0) { SDL_CloseAudioDevice(AudioDev); AudioDev = 0; }
	fflush(stdout);
	fflush(stderr);
	if (SDL_AtomicGet(&GameDone))
	{
		SDL_WaitThread(GameThread, NULL);
		if (WatchdogThread != NULL) SDL_WaitThread(WatchdogThread, NULL);   // sees GameDone within WATCHDOG_MS
		SDL_Quit();
		timeEndPeriod(1);
		exit(code);
	}
	TerminateProcess(GetCurrentProcess(), (UINT)code);
	ExitProcess((UINT)code);   // not reached
}

// Ctrl+C / Ctrl+Break / console close: ask the main loop to quit (exit code
// 0); if it does not within 1 s, terminate from here. Runs on its own thread.
static BOOL WINAPI console_handler(DWORD type)
{
	(void)type;
	SDL_Event e;
	SDL_zero(e);
	e.type = SDL_QUIT;
	SDL_PushEvent(&e);
	Sleep(1000);
	fflush(stdout);
	fflush(stderr);
	TerminateProcess(GetCurrentProcess(), 0);
	return TRUE;
}

// ---------------------------------------------------------------------------

int main(int argc, char* argv[])
{
	cli_options opt;
	char err[1024];
	int rc = cli_parse(argc, argv, &opt, err, (int)sizeof(err));
	if (rc == CLI_HELP)
	{
		printf("Usage: %s.exe [options]\n%s\n", PICOPAD_GAME_NAME, err);
		return 0;
	}
	if (rc != CLI_OK)
	{
		fprintf(stderr, "%s: %s\nUsage: %s.exe [options]\n%s\n",
			PICOPAD_GAME_NAME, err, PICOPAD_GAME_NAME, cli_usage());
		return 2;
	}
	Keys = opt.keys;
	TraceKeys = opt.trace_keys;
	LogMutex = SDL_CreateMutex();   // SDL mutexes work without SDL_Init

	if (opt.headless)
	{
		Headless = 1;
		int code = headless_run(&opt, PICOPAD_GAME_NAME);
		fflush(stdout);
		fflush(stderr);
		return code;
	}

	fprintf(stderr, "seed=%u\n", opt.seed);   // reproduce a run with --seed
	fflush(stderr);
	timeBeginPeriod(1);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
	{
		fprintf(stderr, "%s: SDL init failed: %s\n", PICOPAD_GAME_NAME, SDL_GetError());
		return 1;
	}
	FrontMutex = SDL_CreateMutex();
	GameDoneEvent = SDL_RegisterEvents(1);
	SetConsoleCtrlHandler(console_handler, TRUE);

	SDL_Window* window = SDL_CreateWindow(PICOPAD_GAME_NAME,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		PLAT_W * opt.scale, PLAT_H * opt.scale, 0);
	if (window == NULL)
	{
		fprintf(stderr, "%s: cannot create window: %s\n", PICOPAD_GAME_NAME, SDL_GetError());
		return 1;
	}
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (renderer == NULL) renderer = SDL_CreateRenderer(window, -1, 0);
	if (renderer == NULL)
	{
		fprintf(stderr, "%s: cannot create renderer: %s\n", PICOPAD_GAME_NAME, SDL_GetError());
		return 1;
	}
	SDL_RendererInfo rinfo;
	int vsync = (SDL_GetRendererInfo(renderer, &rinfo) == 0) && (rinfo.flags & SDL_RENDERER_PRESENTVSYNC);
	SDL_RenderSetLogicalSize(renderer, PLAT_W, PLAT_H);
	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
		SDL_TEXTUREACCESS_STREAMING, PLAT_W, PLAT_H);
	if (texture == NULL)
	{
		fprintf(stderr, "%s: cannot create texture: %s\n", PICOPAD_GAME_NAME, SDL_GetError());
		return 1;
	}
	FrontDirty = 1;   // black screen until the game draws

	host_config cfg;
	cfg.virtual_clock = 0;
	cfg.fast_display = opt.fast_display;
	cfg.seed = opt.seed;
	cfg.audio_rate = audio_open();   // CurrentFreq is set from it in RuntimeInit
	cfg.audio_out = (AudioDev != 0) ? PLAT_AUDIO_RING : 0;
	host_configure(&cfg);
	if (AudioDev != 0) SDL_PauseAudioDevice(AudioDev, 0);

	GameThread = SDL_CreateThreadWithStackSize(game_thread, "game", GAME_STACK_SIZE, NULL);
	if (GameThread == NULL)
	{
		fprintf(stderr, "%s: cannot create game thread: %s\n", PICOPAD_GAME_NAME, SDL_GetError());
		return 1;
	}
	WatchdogThread = SDL_CreateThread(watchdog_thread, "watchdog", NULL);
	if (WatchdogThread == NULL)
		plat_logf("watchdog: cannot create thread (%s), stall protection off", SDL_GetError());

	const unsigned long long start = plat_now_us();
	const unsigned long long deadline = opt.max_time_ms ? start + (unsigned long long)opt.max_time_ms * 1000ull : 0;
	unsigned long long last_present = 0;
	unsigned long long pending_since = 0;   // real time an exit became pending (0 = none)
	int redraw = 1;

	for (;;)
	{
		// events (key events update the tap latch one by one)
		SDL_Event e;
		int got = SDL_WaitEventTimeout(&e, EVENT_WAIT_MS);
		while (got)
		{
			switch (e.type)
			{
			case SDL_QUIT:   // window close; SDL turns Ctrl+C (SIGINT) into SDL_QUIT
				finish(0);
				break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				if (e.key.keysym.scancode >= 0 && e.key.keysym.scancode < SDL_NUM_SCANCODES)
				{
					ScanDown[e.key.keysym.scancode] = (e.type == SDL_KEYDOWN);
					if (e.type == SDL_KEYDOWN) ScanKey[e.key.keysym.scancode] = e.key.keysym.sym;
					keys_update();
				}
				break;
			case SDL_WINDOWEVENT:
				if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					memset(ScanDown, 0, sizeof(ScanDown));   // no stuck keys
					keys_update();
				}
				else if (e.window.event == SDL_WINDOWEVENT_EXPOSED ||
					 e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
					redraw = 1;
				break;
			default:
				break;
			}
			got = SDL_PollEvent(&e);
		}
		keys_update();

		const unsigned long long now = plat_now_us();
		const int done = SDL_AtomicGet(&GameDone);

		// present the latest front buffer
		SDL_LockMutex(FrontMutex);
		int dirty = FrontDirty;
		if (dirty && (done || vsync || now - last_present >= NOVSYNC_FRAME_US))
		{
			SDL_UpdateTexture(texture, NULL, Front, PLAT_W * (int)sizeof(unsigned short));
			FrontDirty = 0;
			redraw = 1;
		}
		SDL_UnlockMutex(FrontMutex);
		if (redraw)
		{
			redraw = 0;
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
			last_present = now;
		}

		if (done) finish(SDL_AtomicGet(&GameCode));
		// exit requested off the game thread (fatal error in a watchdog step)
		// but the game never pumps again: end it after PENDING_KILL_US
		if (host_pending_exit() >= 0)
		{
			if (pending_since == 0) pending_since = now;
			else if (now - pending_since >= PENDING_KILL_US) finish(host_pending_exit());
		}
		if (deadline != 0 && plat_now_us() >= deadline) finish(3);
	}
}
