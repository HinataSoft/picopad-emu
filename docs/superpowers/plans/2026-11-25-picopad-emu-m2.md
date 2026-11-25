# Picopad emulátor – milník 2 (headless režim) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Každé herní .exe umí běžet bez okna a zvuku (`--headless`) ve virtuálním čase, řízené skriptem, s ukládáním snímků do PNG a WAV, deterministicky (stejný skript + seed = bajtově stejné PNG) a bez zaseknutí; navíc se `picopad.cmake` dá použít z cizího repa (příprava na picopad-games).

**Architecture:** Host vrstva dostane tři nové věci: mixer zvuku řízený emulovaným časem (místo plnění bufferu do cílové úrovně), háček `plat_tick()` volaný před každým SysTickem (skript, snímky, ukončení) a ochranu proti zaseknutí (posun virtuálního času po N pumpách bez postupu + hlídací volání `host_watchdog()` z jiného vlákna). Platformní vrstva dostane čistě C moduly pro skript, PNG a WAV a headless smyčku v `platform/headless.c`; `main.cpp` podle `--headless` vybere okno, nebo headless. Hra a SDK se nemění.

**Tech Stack:** jako milník 1 – WinLibs GCC 16.2 i686, SDL2 2.32.10, CMake ≥ 3.21 + Ninja, CTest; Python (`C:\Progs\anaconda3\python.exe -I`) jen v testech na dekódování PNG. Žádná nová stahovaná knihovna (PNG kodér je vlastní, bez komprese).

**Spec:** docs/superpowers/specs/2026-11-23-picopad-emulator-design.md (milník 2; části Časování, Zvuk, Rozhraní příkazové řádky). Předchozí plán: docs/superpowers/plans/2026-11-23-picopad-emu-m1.md.

## Global Constraints

- PicoLibSDK se nemění a build do jeho stromu nic nezapisuje; zdrojáky her se nemění, žádné `#ifdef EMULATOR`.
- Překladové jednotky s hlavičkami SDK (host/, testy host vrstvy) nesmí includovat SDL, `windows.h`, `pthread.h` ani `<stdint.h>`; `include/plat_api.h` zůstává v čistém C s vestavěnými typy. Nové platformní moduly `platform/script.c`, `png.c`, `wav.c` jsou čisté C bez SDL a bez SDK (testovatelné samostatně); `platform/headless.c` je C bez SDK a smí z SDL použít jen vlákna a čas (`SDL_CreateThread`, `SDL_Delay`, …, bez `SDL_Init`).
- Headless režim: virtuální čas (`virtual_clock = 1`), žádné okno, žádné audio zařízení, `SDL_Init` se nevolá. Výchozí seed v headless je 0, v interaktivním z času.
- Determinismus: v headless režimu nesmí žádná hodnota reálného času ovlivnit běh hry, snímky ani WAV. Jedinou výjimkou je zásah hlídače (`host_watchdog`), po kterém je běh jen „best effort" a koncový řádek logu to uvádí (`stalls=N`, N > 0).
- SysTick perioda 5 ms (`SYSTICK_MS`); `plat_tick(t_us)` se volá na každý tick s jeho emulovaným časem (5000, 10000, …) **před** obsluhou SysTicku, takže `KeyScan` v tomtéž ticku už vidí klávesy nastavené skriptem.
- Ochrana ve virtuálním čase: po `PUMP_IDLE_CALLS = 1000` voláních pumpy bez postupu virtuálního času se virtuální čas posune na další hranici ticku (deterministické).
- Hlídač: `host_watchdog()` volá platforma z vlastního vlákna každých ~20 ms reálného času; když se pumpa nezavolala `STALL_US = 100000` µs reálného času a herní vlákno nedrží IRQ zámek, hlídač provede jeden krok pumpy (ve virtuálním čase posun o jeden tick), zvýší `host_stall_count()` a při prvním zásahu v epizodě zapíše přes `plat_log` řádek začínající `stall`.
- Mixer zvuku (`PWMSndIrq` z SDK, beze změny) se volá **podle emulovaného času**: po pumpě je vyrobeno přesně `floor(t_us * rate / 1000000)` vzorků (v režimu zvukového zařízení navíc s předstihem `AUDIO_TARGET = 2048` vzorků). Běží jen na herním vlákně, nebo z hlídače pod zámkem pumpy – nikdy souběžně.
- `rate` mixeru = `host_config.audio_rate`, 0 znamená 44100. `CurrentFreq[CLK_SYS] = 2048 * rate` (beze změny proti M1).
- Převod vzorku: `(samp-1024)*32` saturovaný na s16 (beze změny). WAV: RIFF PCM, mono, 16 bit, little endian, `rate` Hz.
- PNG: 320×240, barevný typ 2 (RGB 8 bit), bez prokládání, IDAT jako zlib s nekomprimovanými („stored") deflate bloky, správné CRC32 a Adler-32. Převod RGB565: `r8 = (r5<<3)|(r5>>2)`, `g8 = (g6<<2)|(g6>>4)`, `b8 = (b5<<3)|(b5>>2)`. Snímek je obsah displeje, jak ho viděl `plat_present` (ne `FrameBuf`).
- Ukončovací kódy: 0 = `quit` ve skriptu, `ResetToBootLoader`, zavření okna / Ctrl+C; 1 = fatální chyba (`panic`, chyba zápisu PNG/WAV); 2 = chybné argumenty nebo chyba ve skriptu; 3 = vypršel `--max-time` (v headless virtuální čas, výchozí 60 s).
- Headless log jde na **stdout**, jeden řádek = jedna událost, tvar `slovo klíč=hodnota …` (formáty v Task 7); hlášky hry a `plat_log` dál na stderr.
- `--wav` je povolené jen s `--headless` (jinak chyba, kód 2).

## Review Focus

1. **Headless hra bez skriptu čeká na klávesu** (`while (KeyGet() == NOKEY) {}`) – nesmí viset: virtuální čas postupuje přes `PUMP_IDLE_CALLS`, běh skončí na `--max-time` s kódem 3 a řádkem `end reason=max-time`. Test v Task 7 (hra KEYWAIT bez skriptu).
2. **Smyčka bez volání SDK** (`while (PlayingSound()) {}` jako PACMAN/SOKOBAN/VEGASLOT) – v headless i interaktivně doběhne díky hlídači a mixeru řízenému časem; `stalls` > 0. Test v Task 3 (unit) a Task 7 (hra STALL).
3. **Hra skončí dřív, než skript doběhne** (`ResetToBootLoader` před posledním `shot`) – kód 0, `end reason=exit … pending=N` s N > 0, žádný pád ani prázdný PNG. Test v Task 7.
4. **Neexistující nebo vnořený `--shot-dir`** – adresáře se vytvoří; nezapisovatelná cesta = kód 1 s hláškou na stderr, ne tichý úspěch. Test v Task 7.
5. **Stisk a uvolnění klávesy ve stejném ticku** (`500 press A` + `500 release A`, nebo `hold A 1`) – stisk se nesmí ztratit: klávesa je hlášená stisknutá aspoň po jeden tick. Test v Task 5.

---

## Struktura souborů (nové a měněné)

```
include/plat_api.h        # + audio_out, plat_tick, plat_audio_write, host_watchdog, host_stall_count
host/audio.c              # mixer podle času, ring jen v režimu zařízení, záchyt vzorků
host/pump.c               # plat_tick před SysTickem, PUMP_IDLE_CALLS, zámek pumpy, host_watchdog
host/clock.c              # posun virtuálního času na další tick (pro pump.c)
host/runtime.c            # ukončení z plat_tick (longjmp s kódem), čekající kód z hlídače
platform/script.c/.h      # parser a přehrávač skriptu (čisté C)
platform/png.c/.h         # zápis PNG bez komprese (čisté C)
platform/wav.c/.h         # zápis WAV (čisté C)
platform/headless.c/.h    # headless běh: plat_tick, snímky, WAV, log (čisté C)
platform/cli.c/.h         # --headless --script --shot-dir --wav --trace-keys
platform/main.cpp         # výběr režimu, hlídací vlákno, plat_audio_write/plat_tick pro okno
cmake/picopad.cmake       # samostatně použitelný z cizího projektu
cmake/PicopadEmu.cmake    # vstupní bod pro cizí projekt (SDL2, cesty, toolchain)
cmake/toolchain-mingw32.cmake
tests/plat_fake.c/.h      # + plat_tick, plat_audio_write
tests/test_tick.c, test_stall.c, test_script.c, test_png.c, test_wav.c
tests/games/KEYWAIT/, tests/games/STALL/   # malé testovací hry (vlastní Makefile jako HELLO)
tests/headless/*.txt      # skripty; tests/ref/*.png referenční snímky
tests/external/CMakeLists.txt              # cizí projekt, který staví HELLO přes PicopadEmu.cmake
```

## Rozhraní host ↔ platform – změny v `include/plat_api.h`

Používají ho Task 1–3 a 7; žádná jiná jména se nevymýšlí.

```c
#define PLAT_AUDIO_RING    1   /* feed the ring drained by host_audio_render (audio device) */
#define PLAT_AUDIO_CAPTURE 2   /* pass every sample to plat_audio_write */

typedef struct {
    int virtual_clock;
    int fast_display;
    unsigned int seed;
    int audio_rate;            /* mixer rate Hz; 0 = 44100 */
    int audio_out;             /* PLAT_AUDIO_* bits; 0 = mixer runs, samples discarded */
} host_config;

/* platform, called by host (game thread, or the watchdog thread under the pump lock) */
int  plat_tick(unsigned long long t_us);            /* before each SysTick; <0 = go on, >=0 = end game with this exit code */
void plat_audio_write(const short* s, int n);       /* PLAT_AUDIO_CAPTURE samples, in order */

/* host, called by platform */
void host_watchdog(void);                           /* from a non-game thread every ~20 ms */
unsigned int host_stall_count(void);                /* watchdog interventions since host_run_game start */
```

---

### Task 1: Zvuk řízený emulovaným časem

**Files:**
- Modify: `include/plat_api.h`, `host/audio.c`, `host/runtime.c` (výchozí rate), `platform/main.cpp` (`audio_out = PLAT_AUDIO_RING` při otevřeném zařízení, prázdné `plat_audio_write`), `tests/plat_fake.c/.h`, `tests/test_audio.c`

**Interfaces:**
- Consumes: `host_pump` → `host_audio_pump()` (M1), `host_now_us()`.
- Produces: `host_config.audio_out`, `plat_audio_write`; `host_audio_pump()` vyrobí vzorky do `floor(now*rate/1e6)` (+ `AUDIO_TARGET` při `PLAT_AUDIO_RING`); bez `PWM_Enable` nebo bez handleru se počítadlo vyrobených vzorků posouvá a vzorky jsou 0 (ticho, ale čas běží). Při `PLAT_AUDIO_RING` se do ringu zapisuje, jen dokud je v něm méně než `2*AUDIO_TARGET` vzorků (jinak se vzorek zahodí – omezená latence), podtečení dál opakuje poslední hodnotu. Počítadlo se nuluje v `host_run_game`.
- plat_fake: `fake_audio[]` (prvních 65536 vzorků) a `fake_audio_count`.

- [ ] **Step 1: Testy v `tests/test_audio.c`** (virtuální čas, `audio_rate = 44100`, `audio_out = PLAT_AUDIO_CAPTURE`):

```c
#define DUE(t) ((int)((unsigned long long)(t) * 44100 / 1000000))
u64 t0 = Time64();                           /* RuntimeInit/DeviceInit may already have advanced time */
CHECK_EQ(fake_audio_count, DUE(t0));
int c0 = fake_audio_count;
PlaySound(snd, sizeof(snd));                 /* 4410 B @22050 Hz = 0.2 s */
WaitMs(100);
CHECK_EQ(fake_audio_count, DUE(t0 + 100000));
CHECK(PlayingSound());
WaitMs(150);
CHECK(!PlayingSound());                      /* no audio device needed */
CHECK_EQ(fake_audio_count, DUE(t0 + 250000));
CHECK(fake_audio[c0 + 100] > 0);             /* 0xC0 samples (fake_audio keeps the first 65536) */
```

  Druhý běh s `audio_out = 0`: `PlayingSound()` po `WaitMs(250)` také skončí a `fake_audio_count` zůstane 0. Stávající test souběhu s render vláknem upravit na `audio_out = PLAT_AUDIO_RING` a reálný čas z `fake_now`; musí dál projít.
- [ ] **Step 2: Spustit** `ctest --test-dir build -R audio` – Expected: FAIL (`fake_audio_count` neexistuje / 0)
- [ ] **Step 3: Implementovat** změny v `plat_api.h`, `audio.c`, `runtime.c`, `main.cpp`, `plat_fake`
- [ ] **Step 4: Spustit** celé `ctest --test-dir build` – Expected: 14/14 PASS, `tetris_smoke` dál hraje zvuk
- [ ] **Step 5: Commit** `git commit -m "host: time-driven sound mixer with sample capture"`

---

### Task 2: Háček `plat_tick` a ukončení hry z platformy

**Files:**
- Modify: `include/plat_api.h`, `host/pump.c`, `host/runtime.c`, `host/host_internal.h`, `tests/plat_fake.c/.h`, `platform/main.cpp` (interaktivně `plat_tick` vrací -1)
- Create: `tests/test_tick.c` (+ registrace `picopad_host_test(tick)`)

**Interfaces:**
- Consumes: smyčka ticků v `host_pump` (M1).
- Produces: v `host_pump` před každým `SysTickHandler()` volání `plat_tick(LastTick)` (už posunutého na čas ticku); návratová hodnota ≥ 0 na herním vlákně → `host_exit(code)` (nová funkce v `runtime.c`, `noreturn`, longjmp; `host_run_game` vrátí `code`). Z jiného vlákna (hlídač, Task 3) se kód uloží a provede při příští pumpě na herním vlákně.
- plat_fake: `fake_tick_times[64]`, `fake_tick_count`, `fake_tick_fn` (ukazatel na funkci testu, která dostane `t_us` a vrátí kód; NULL = -1).

- [ ] **Step 1: `tests/test_tick.c`** (virtuální čas):

```c
/* fake_tick_fn: t == 100000 -> fake_keys = PLAT_KEY_BIT(KEY_A); t == 200000 -> return 7 */
while (Time() < 50000) WaitMs(5);
for (i = 0; i < fake_tick_count; i++)
    CHECK_EQ(fake_tick_times[i], 5000ULL * (i + 1));   /* every tick, from host_run_game start */
CHECK(fake_tick_count >= 10);
while (Time() < 100000) { CHECK_EQ(KeyGet(), NOKEY); WaitMs(5); }
WaitMs(20);                             /* A set in tick 100000 before KeyScan, debounce settles */
CHECK_EQ(KeyGet(), KEY_A);
WaitMs(200);                            /* returns 7 at tick 200000 */
CHECK(0);                               /* not reached */
/* main(): CHECK_EQ(host_run_game(), 7); a druhý host_run_game() se stejným fake_tick_fn znovu vrátí 7 a ticky začnou znovu od 5000 (reset stavu) */
```
- [ ] **Step 2: Spustit** `ctest --test-dir build -R tick` – Expected: FAIL (link: `plat_tick`)
- [ ] **Step 3: Implementovat**
- [ ] **Step 4: Spustit** celé `ctest` – Expected: PASS
- [ ] **Step 5: Commit** `git commit -m "host: plat_tick hook before SysTick, exit code from platform"`

---

### Task 3: Ochrana proti zaseknutí

**Files:**
- Modify: `host/pump.c`, `host/clock.c`, `host/host_internal.h`, `include/plat_api.h`, `platform/main.cpp` (hlídací vlákno přes `SDL_CreateThread` ~20 ms; v headless ho spustí Task 7)
- Create: `tests/test_stall.c` (+ `tests/thread_helper.c` pro pomocné vlákno)

**Interfaces:**
- Consumes: `plat_tick` a čekající kód z Task 2, mixer podle času z Task 1, `host_irq_lock` (M1).
- Produces:
  - **zámek pumpy**: `InPump` se stává atomickým zámkem (CAS) – pumpa herního vlákna a hlídač se vzájemně vylučují; vnořené volání na herním vlákně se dál vrací hned;
  - `PumpSeq` – atomický čítač, každá pumpa ho zvýší (herní vlákno kvůli němu nečte reálný čas);
  - virtuální čas: `PUMP_IDLE_CALLS = 1000` pump bez změny `host_now_us()` → `host_clock_advance_to_next_tick()` (nová funkce v clock.c, posune `VirtNow` na `LastTick + 5000`) a čítač se vynuluje; čekání (`host_wait_us`) čítač nuluje také;
  - `host_watchdog()`: běží jen když je hra uvnitř `host_run_game`; pamatuje si `PumpSeq` a `plat_now_us()` posledního pozorovaného postupu; nezměnil-li se `PumpSeq` po dobu `STALL_US = 100000` µs, zkusí zámek pumpy a IRQ zámek **bez čekání** (obsazené = tento pokus přeskočit), ve virtuálním čase posune čas o tick, provede tělo pumpy (ticky s `plat_tick`, mixer), zvýší `StallCount`, při prvním zásahu epizody `plat_log("stall: game loop without SDK calls, advancing time")`; vlastní krok hlídače `PumpSeq` nezvyšuje; epizoda končí, když se `PumpSeq` znovu pohne (hra zase volá SDK); během epizody hlídač dělá jeden krok při každém volání;
  - `host_stall_count()`.

- [ ] **Step 1: `tests/test_stall.c`**:

```c
/* game (virtual clock, audio_out = 0): */
PlaySound(snd, sizeof(snd));                 /* 0.2 s */
while (PlayingSound()) {}                    /* no SDK call that pumps */
stall_loop_done = 1;
/* helper thread: until done or 5 s real: fake_now += 20000; host_watchdog(); Sleep-free loop using thr_* helper */
/* main(): CHECK_EQ(host_run_game(), 0); CHECK(stall_loop_done); CHECK(host_stall_count() >= 1);
           CHECK(strncmp(fake_log, "stall", 5) == 0); */
```

  Druhá část (deterministická, bez hlídače): `u32 t0 = Time(); while (Time() - t0 < 50000) {}` ve virtuálním čase doběhne a `host_stall_count() == 0`; a `for (i = 0; i < 5000; i++) dmb();` posune `Time()` o 25000 µs (5 × 1000 pump → 5 ticků).
  `fake_now` musí být pro sdílení mezi vlákny `volatile` (úprava plat_fake).
- [ ] **Step 2: Spustit** `ctest --test-dir build -R stall` – Expected: FAIL (test se zasekne → timeout 30 s, nebo link `host_watchdog`)
- [ ] **Step 3: Implementovat**
- [ ] **Step 4: Spustit** celé `ctest` – Expected: PASS (včetně `clock`, `audio`)
- [ ] **Step 5: Commit** `git commit -m "host: virtual-time idle advance and stall watchdog"`

---

### Task 4: Zápis PNG a WAV

**Files:**
- Create: `platform/png.c/.h`, `platform/wav.c/.h`, `tests/test_png.c`, `tests/test_wav.c`, `tests/png_decode.py`
- Modify: `CMakeLists.txt` (testy `png`, `wav`; `png_decode` jen když je nalezen `PICOPAD_PYTHON`)

**Interfaces:**
- Produces:
  - `int png_write_rgb565(const char* path, const unsigned short* fb, int w, int h, int stride);` – 0 = OK, -1 = chyba (errno zachované). Formát podle Global Constraints.
  - `typedef struct wav_writer wav_writer;` `wav_writer* wav_open(const char* path, int rate);` `int wav_write(wav_writer*, const short* s, int n);` `int wav_close(wav_writer*);` – `wav_close` doplní velikosti v hlavičce; zápis i zavření vrací 0/-1. Vlákno: volá se z jednoho vlákna naráz (zajišťuje volající).

- [ ] **Step 1: Testy.** `test_png.c` zapíše 320×240 vzorek (pixel = `(x*31/319)<<11 | (y*63/239)<<5 | ((x+y)&31)`) a 2×1 obrázek `{0xF800, 0x07E0}` a zkontroluje signaturu, IHDR (`w`, `h`, bit depth 8, color type 2) a CRC IHDR. `tests/png_decode.py <png> <x> <y> <r> <g> <b> …` dekóduje přes `zlib.decompress` (filtr 0 na každém řádku) a ověří pixely: pro 2×1 `(0,0) = 255,0,0`, `(1,0) = 0,255,0`; pro velký vzorek rohy a střed spočítané podle převodu z Global Constraints. `test_wav.c`: 1000 vzorků @22050 → velikost souboru 44 + 2000, `RIFF` velikost 2036, `data` velikost 2000, `fmt` = PCM 1 kanál 22050 Hz 16 bit; `wav_open` do neexistujícího adresáře vrátí NULL.
- [ ] **Step 2: Spustit** `ctest --test-dir build -R "png|wav"` – Expected: FAIL
- [ ] **Step 3: Implementovat**
- [ ] **Step 4: Spustit** – Expected: PASS
- [ ] **Step 5: Commit** `git commit -m "platform: uncompressed PNG and WAV writers"`

---

### Task 5: Skript – parser a přehrávač

**Files:**
- Create: `platform/script.c/.h`, `tests/test_script.c` (+ registrace v CMakeLists.txt jako čisté C)

**Interfaces:**
- Produces:

```c
typedef enum { SCR_PRESS, SCR_RELEASE, SCR_SHOT, SCR_QUIT } script_op;
typedef struct { long long t_ms; script_op op; unsigned int key_bit; char name[64]; int line; } script_event;
typedef struct { script_event* ev; int count; int next; unsigned int keys; unsigned int pressed_this_tick; } script;

int  script_parse(const char* text, script* out, char* err, int errlen);  /* 0 = OK; err "line N: …" */
int  script_load(const char* path, script* out, char* err, int errlen);   /* reads file, then script_parse */
void script_free(script* s);
/* events with t_ms*1000 <= t_us, in file order; keys = current key bits; shot names / quit reported via callbacks */
typedef struct { void (*shot)(void* ctx, const char* name); void* ctx; } script_sink;
int  script_step(script* s, unsigned long long t_us, const script_sink* sink); /* returns 1 when quit reached */
int  script_pending(const script* s);
```

- Syntaxe: řádek = `<ms> <příkaz>`; `#` začíná komentář; prázdné řádky se ignorují; příkazy `press K`, `release K`, `hold K MS` (= press v čase t a release v t+MS), `shot JMÉNO` (`[A-Za-z0-9_-]{1,63}`), `quit`. Klávesy `UP DOWN LEFT RIGHT A B X Y` (bez ohledu na velikost písmen) → `PLAT_KEY_BIT(KEY_…)` s kódy z plat_api.h (UP=1 … B=8). Časy nemusí být seřazené; události se stabilně seřadí podle času (`hold` vloží release na své místo).
- Pravidlo stisku: release, který připadne na stejný `script_step` jako press téže klávesy, se odloží na další krok (klávesa je stisknutá aspoň jeden tick).

- [ ] **Step 1: `tests/test_script.c`**:

```c
CHECK_EQ(script_parse("500 press A\n600 release a\n# c\n\n2000 shot title\n2000 hold RIGHT 300\n9000 quit\n", &s, err, 128), 0);
CHECK_EQ(s.count, 6);                                   /* hold -> press + release */
script_step(&s, 499000, &sink); CHECK_EQ(s.keys, 0);
script_step(&s, 500000, &sink); CHECK_EQ(s.keys, PLAT_KEY_BIT(7));
script_step(&s, 600000, &sink); CHECK_EQ(s.keys, 0);
script_step(&s, 2000000, &sink); CHECK_STR(last_shot, "title"); CHECK_EQ(s.keys, PLAT_KEY_BIT(3));
script_step(&s, 2300000, &sink); CHECK_EQ(s.keys, 0);
CHECK_EQ(script_step(&s, 9000000, &sink), 1);
/* same-tick tap (Review Focus 5) */
script_parse("500 press A\n500 release A\n", &s2, err, 128);
script_step(&s2, 500000, &sink); CHECK_EQ(s2.keys, PLAT_KEY_BIT(7));
script_step(&s2, 505000, &sink); CHECK_EQ(s2.keys, 0);
/* errors */
CHECK_EQ(script_parse("100 jump A\n", &s3, err, 128), 1); CHECK(strstr(err, "line 1") != NULL);
CHECK_EQ(script_parse("x press A\n", &s3, err, 128), 1);
CHECK_EQ(script_parse("100 press Q\n", &s3, err, 128), 1);
CHECK_EQ(script_parse("100 shot a/b\n", &s3, err, 128), 1);
```
- [ ] **Step 2: Spustit** `ctest --test-dir build -R script` – Expected: FAIL
- [ ] **Step 3: Implementovat**
- [ ] **Step 4: Spustit** – Expected: PASS
- [ ] **Step 5: Commit** `git commit -m "platform: input script parser and player"`

---

### Task 6: Příkazová řádka pro headless

**Files:**
- Modify: `platform/cli.c/.h`, `tests/test_cli.c`

**Interfaces:**
- Produces: nová pole `cli_options`: `int headless; const char* script; const char* shot_dir; const char* wav; int trace_keys; int max_time_set;` Výchozí: `shot_dir = "."`, v headless bez `--max-time` platí `max_time_ms = 60000`; seed v headless bez `--seed` = 0. Chyby (`CLI_ERROR`): `--wav` bez `--headless`, `--script` / `--shot-dir` / `--wav` bez hodnoty. `--scale` a `--keys` jsou v headless povolené a ignorované. Usage text doplnit.

- [ ] **Step 1: Testy v `tests/test_cli.c`**: `--headless` → `headless == 1`, `max_time_ms == 60000`, `seed == 0`; `--headless --max-time 5s --seed 9` → 5000, 9; `--headless --script s.txt --shot-dir out --wav a.wav --trace-keys` → řetězce a příznak; `--wav a.wav` bez headless → `CLI_ERROR` se zmínkou `--headless` v `err`; `--script` na konci → `CLI_ERROR`; bez `--headless` zůstává `max_time_ms == 0`.
- [ ] **Step 2: Spustit** `ctest --test-dir build -R cli` – Expected: FAIL
- [ ] **Step 3: Implementovat**
- [ ] **Step 4: Spustit** – Expected: PASS
- [ ] **Step 5: Commit** `git commit -m "cli: headless, script, shot-dir, wav, trace-keys options"`

---

### Task 7: Headless běh, log a testovací hry

**Files:**
- Create: `platform/headless.c/.h`, `tests/games/KEYWAIT/` (`Makefile`, `config.h`, `include.h`, `src/main.cpp` – stejné uspořádání jako `examples/HELLO`), `tests/games/STALL/` (totéž), `tests/headless/*.txt`, `tests/headless_test.cmake`, `tests/ref/hello_title.png`
- Modify: `platform/main.cpp`, `CMakeLists.txt`, `README.md`

**Interfaces:**
- Consumes: Task 1–6.
- Produces:
  - `int headless_run(const cli_options* opt, const char* game_name);` (vrací ukončovací kód; startuje herní vlákno a hlídací vlákno přes SDL vlákna), a headless implementace `plat_tick`, `plat_audio_write`, `plat_key_bits`, `plat_present` (kopie do vlastního bufferu displeje), na které `main.cpp` deleguje, je-li `opt.headless`.
  - `plat_tick` v headless: `script_step` (klávesy, snímky), `--trace-keys` řádek při změně kláves, konec při `quit` (kód 0) nebo `t_us >= max_time_ms*1000` (kód 3). Při volání z hlídače (hra visí v smyčce bez SDK) headless smyčka po vráceném kódu počká 1 s reálného času na konec herního vlákna a pak proces ukončí sama (`TerminateProcess` přes ukazatel z main.cpp) s týmž kódem po vypsání `end` řádku a zavření WAV.
  - Log na stdout (každý řádek hned `fflush`):
    - `start game=<jméno> seed=<n> rate=<hz>`
    - `shot t=<ms> file=<cesta>`
    - `keys t=<ms> bits=0x<hex2>` (jen s `--trace-keys`)
    - `end reason=quit|exit|fatal|max-time t=<ms> code=<n> stalls=<n> pending=<n>`
    - `t` = emulovaný čas v ms; `exit` = hra skončila sama (`ResetToBootLoader` nebo návrat z main), `pending` = počet nezpracovaných událostí skriptu.
  - Interaktivní režim navíc vypíše na stderr `seed=<n>` při startu (reprodukovatelnost), spouští hlídací vlákno z Task 3 a s `--trace-keys` jeho `plat_tick` vypisuje na stderr stejné `keys` řádky.
  - `--shot-dir` se vytvoří včetně rodičů; chyba zápisu PNG nebo WAV → hláška na stderr, `end reason=fatal`, kód 1.
  - Testovací hry: **KEYWAIT** nakreslí text „PRESS A", čeká `while (KeyGet() == NOKEY) {}`, po A nakreslí zelený obdélník, `DispUpdate()`, `WaitMs(100)` a `ResetToBootLoader()`. **STALL** spustí krátký zvuk, `while (PlayingSound()) {}`, nakreslí obdélník a `ResetToBootLoader()`.
- ctest (přes `tests/headless_test.cmake`, který spustí exe, zkontroluje kód, řádky stdout a soubory):
  - `keywait_script`: skript `200 shot before`, `300 hold A 100`, `1000 shot after`, `2000 quit`; hra skončí sama krátce po stisku A, takže se očekává kód 0, `end reason=exit code=0 … pending=2` (`shot after` a `quit` nezpracované), existující `before.png` a neexistující `after.png` (Review Focus 3).
  - `keywait_noscript`: `--headless --max-time 3s` → kód 3, `end reason=max-time t=3000`.
  - `stall_headless`: kód 0, `end reason=exit`, `stalls=` > 0.
  - `hello_headless`: `200 shot title`, `300 quit` → kód 0, `title.png` bajtově shodný s `tests/ref/hello_title.png`.
  - `shot_dir_nested`: `--shot-dir <build>/shots/a/b` se vytvoří; `shot_dir_bad`: `--shot-dir` na existující **soubor** → kód 1.
  - `script_error`: skript s neplatným řádkem → kód 2, hláška `line N` na stderr.
  - `wav_headless`: KEYWAIT se skriptem `300 hold A 100` a `--wav` → hlavička WAV má platné velikosti (RIFF = soubor − 8, data = soubor − 44), počet vzorků = data/2 se od `t*44.1` z `end` řádku liší nejvýš o 221 (jeden tick).
- [ ] **Step 1: Napsat testovací hry, skripty a `tests/headless_test.cmake`; registrovat testy**
- [ ] **Step 2: Spustit** `ctest --test-dir build -R "headless|keywait|stall_|shot_dir|script_error|wav_"` – Expected: FAIL (`--headless` neznámá volba → kód 2)
- [ ] **Step 3: Implementovat `platform/headless.c` a dispatch v `main.cpp`**
- [ ] **Step 4: Vygenerovat `tests/ref/hello_title.png`** z prvního zeleného běhu a ručně prohlédnout (Read tool na PNG): obsahuje RGB čtverečky, „HELLO PICOPAD" a zelený obdélník
- [ ] **Step 5: Spustit** celé `ctest` – Expected: PASS
- [ ] **Step 6: README** – sekce „Headless režim" (volby, formát skriptu, formát logu, kódy, omezení determinismu po `stall`); ze „Známých omezení" odstranit body vyřešené tímto milníkem
- [ ] **Step 7: Commit** `git commit -m "platform: headless mode with script, PNG shots, WAV and log"`

---

### Task 8: Determinismus TETRISu (akceptační test milníku)

**Files:**
- Create: `tests/headless/tetris.txt`, `tests/ref/tetris_*.png`, `tests/determinism_test.cmake`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: headless běh z Task 7.
- Produces: skript pro TETRIS – úvodní obrazovka, start hry, několik pohybů a otočení (`hold LEFT/RIGHT/UP/DOWN 100`, `hold A 100`) přes ~10 s, alespoň 3 snímky (`title`, `game1`, `game2`) a `quit`; konkrétní klávesy a časy zvolí implementátor podle ovládání TETRISu (zdroják `PicoPad/GAME/TETRIS`), aby snímky `game1` a `game2` ukazovaly rozehranou hru s různým stavem.
- ctest `tetris_determinism`: `determinism_test.cmake` spustí TETRIS headless se skriptem **dvakrát** do dvou různých adresářů se `--seed 1`, oba kódy 0, `stalls=0`, každý snímek bajtově shodný mezi běhy (`cmake -E compare_files`) a shodný s `tests/ref/`. Při rozdílu proti `ref` vypíše, které soubory se liší a jak referenci obnovit (zkopírovat z build adresáře). Třetí běh se `--seed 2` musí dát aspoň jeden snímek `game*` odlišný (seed opravdu působí).

- [ ] **Step 1: Napsat skript a `determinism_test.cmake`, registrovat test**
- [ ] **Step 2: Spustit** – Expected: FAIL (chybí `tests/ref/tetris_*.png`)
- [ ] **Step 3: Vygenerovat reference, prohlédnout je (Read tool) – titulní obrazovka a dvě rozehrané pozice – a commitnout**
- [ ] **Step 4: Spustit** `ctest --test-dir build -R tetris_determinism` třikrát po sobě – Expected: PASS pokaždé
- [ ] **Step 5: Commit** `git commit -m "test: TETRIS headless determinism with reference PNGs"`

---

### Task 9: `picopad.cmake` pro cizí projekty

**Files:**
- Create: `cmake/PicopadEmu.cmake`, `cmake/toolchain-mingw32.cmake`, `tests/external/CMakeLists.txt`, `tests/external_test.cmake`
- Modify: `cmake/picopad.cmake`, `CMakeLists.txt`, `CMakePresets.json` (preset použije toolchain soubor), `README.md`

**Interfaces:**
- Produces:
  - `cmake/toolchain-mingw32.cmake`: `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, `CMAKE_ASM_COMPILER`, `CMAKE_MAKE_PROGRAM` z `PICOPAD_TOOLCHAIN_DIR` (výchozí `<repo emulátoru>/../toolchain`).
  - `cmake/PicopadEmu.cmake`: po `project()` cizího projektu nastaví výchozí `PICOPAD_TOOLCHAIN_DIR` a `PICOLIBSDK_DIR` relativně k **repu emulátoru** (ne k `CMAKE_SOURCE_DIR` cizího projektu), najde SDL2 (stejně jako dnešní CMakeLists.txt), includuje `picopad.cmake`; opakované includnutí nevadí.
  - `picopad.cmake`: relativní cesty `game_dir`/`config_dir` se berou od `CMAKE_CURRENT_SOURCE_DIR` volajícího, ne od `CMAKE_SOURCE_DIR`; nic nezávisí na proměnných definovaných jen v kořenovém CMakeLists.txt emulátoru.
  - Hlavní `CMakeLists.txt` emulátoru používá `PicopadEmu.cmake` sám (jedna cesta, žádná duplicita s hledáním SDL2).
  - Použití v cizím repu (vzor pro picopad-games, uvést v README):

```cmake
cmake_minimum_required(VERSION 3.21)
project(mygames C CXX ASM)
include(${PICOPAD_EMU_DIR}/cmake/PicopadEmu.cmake)
picopad_add_game(HELLO games/HELLO)
```

- ctest `external_project` (label `slow`): `external_test.cmake` nakonfiguruje `tests/external` (generátor Ninja, `-DCMAKE_TOOLCHAIN_FILE=…/toolchain-mingw32.cmake -DPICOPAD_EMU_DIR=<repo>`) do `<build>/external`, sestaví ho a spustí výsledné `HELLO.exe --headless --max-time 1s` s očekávaným kódem 3.
- [ ] **Step 1: Napsat `tests/external/CMakeLists.txt` a `external_test.cmake`, registrovat test**
- [ ] **Step 2: Spustit** `ctest --test-dir build -R external_project` – Expected: FAIL (`PicopadEmu.cmake` neexistuje)
- [ ] **Step 3: Implementovat**; čistý `cmake --preset default` + build + celé `ctest` musí dál projít
- [ ] **Step 4: Spustit** celé `ctest` – Expected: PASS
- [ ] **Step 5: README** – sekce „Použití z jiného repa"
- [ ] **Step 6: Commit** `git commit -m "cmake: PicopadEmu.cmake entry point for external game repos"`
