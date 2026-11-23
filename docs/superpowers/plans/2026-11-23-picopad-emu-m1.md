# Picopad emulátor – milník 1 (TETRIS interaktivně) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Přeložit PicoPad/GAME/TETRIS z PicoLibSDK beze změn jako nativní 32bit Windows .exe, který běží v okně SDL2 s klávesnicí a zvukem, a dokázat, že ukázková hra mimo strom SDK se přeloží pro emulátor i pro zařízení.

**Architecture:** Přenositelné knihovny SDK se překládají beze změny; hardwarová vrstva SDK (`_sdk/src`, `st7789.c`, `lib_sd.c`, `picopad_bat.c`, inline registrové hlavičky) se nahrazuje vrstvou `host/`, která se vnucuje přes `-include host_prelude.h` a předdefinované include guardy. Vrstva `platform/` (SDL2, Windows) nevidí hlavičky SDK; s `host/` mluví jen přes `include/plat_api.h` v čistém C. Hra běží na vlastním vlákně, SysTick se spouští "pumpou" na herním vlákně.

**Tech Stack:** WinLibs MinGW-w64 GCC i686 (winget), SDL2 2.x mingw devel balíček (i686-w64-mingw32), CMake 3.31 (C:\Progs\CMake\bin), Ninja, CTest. Python (`C:\Progs\anaconda3\python.exe`) jen pro kontrolu UF2.

**Spec:** docs/superpowers/specs/2026-11-23-picopad-emulator-design.md (milník 1). Milníky 2–4 dostanou vlastní plány po dokončení tohoto.

## Global Constraints

- PicoLibSDK (D:\Work\Prog\Picopad\PicoLibSDK) se nemění a build emulátoru do jeho stromu nic nezapisuje. Cesta se předává CMake proměnnou `PICOLIBSDK_DIR` (výchozí `${CMAKE_SOURCE_DIR}/../PicoLibSDK`).
- Zdrojáky her se nemění; žádné `#ifdef EMULATOR` ve hrách. `main()` hry se přejmenuje jen při překladu: `-Dmain=game_main`.
- 32bit build (i686). Kód SDK: `-std=gnu11` (C), `-std=gnu++17 -fno-exceptions -fno-rtti` (C++), `-funsigned-char -fno-strict-aliasing`, bez `-Werror`.
- Definice pro kód SDK: `RP2040=1 USE_PICOPAD=1 USE_PICOPAD10=1 FLASHSIZE=0x200000 RAMSIZE=0x40000 INCLUDES_H="<SDK>/includes.h" CONFIG_DEF_H="<SDK>/config_def.h" USE_FLOATLIBC=1 USE_DOUBLELIBC=1 USE_CONFIG=0 USE_CANVAS=0 DRAW_HWINTER=0`.
- Překladové jednotky, které includují `global.h` SDK, nesmí includovat SDL, `windows.h` ani `<stdint.h>`; `include/plat_api.h` používá jen vestavěné typy C.
- Displej 320x240 RGB565 (`SDL_PIXELFORMAT_RGB565`), výchozí měřítko okna 3×.
- Model přenosu na displej je výchozí zapnutý: SPI 24 MHz, 16 bitů/px, tj. cena v µs = `w*h*16/24` (celá obrazovka 51200 µs); `--fast-display` ho vypne.
- Klávesy: `--keys=letters` (výchozí) šipky = D-pad, A/B/X/Y = stejnojmenná tlačítka, Esc = Y; `--keys=layout` šipky = D-pad, A = X, S = Y, Z = A, X = B, Esc = Y.
- Zvuk: `CurrentFreq[CLK_SYS] = 2048 * audio_rate` nastavit před `DeviceInit()`; vzorek z `PWM_Comp` (0..2047, 1024 = ticho) převést na s16 jako `(samp-1024)*32`; před prvním `PWM_Enable` posílat ticho.
- Ukončovací kódy .exe: 0 = hra skončila přes `ResetToBootLoader`, 1 = fatální chyba/`panic`, 2 = chybné argumenty, 3 = vypršel `--max-time`.
- Instalace balíčků a stahování (WinLibs, Ninja, SDL2) proběhne až po výslovném souhlasu uživatele.

## Review Focus

1. **Aktivní čekání bez volání hostitele** (`while (KeyGet() == NOKEY) {}`, `while (PlayingSound()) {}`) – hra nesmí zamrznout ani žrát 100 % jádra bez reakce na klávesy; v milníku 1 stačí, že pumpa běží z `dmb()`, `GPIO_In()`, času a USB stubů a že okno zůstává responzivní. Test v Task 3.
2. **Zavření okna nebo Ctrl+C uprostřed hry** – proces musí skončit do 1 s s kódem 0 i když herní vlákno visí ve smyčce. Test v Task 7.
3. **Audio vlákno a herní vlákno současně** (PlaySound z herního vlákna během callbacku) – žádný pád ani "zaseknutý" kanál; IRQ mutex chrání mixer. Test v Task 6.
4. **Současně stisknuté klávesy a rychlé ťuknutí kratší než 5 ms** – stisk se nesmí ztratit (ťuknutí musí vydržet aspoň do dalšího ticku). Test v Task 5.
5. **Částečný dirty obdélník u okraje obrazovky** (x+w = 320, y+h = 240) – kopie do předního bufferu nesmí číst ani psát mimo pole. Test v Task 4.

---

## Struktura souborů

```
picopad-emu/
  CMakeLists.txt                 # projekt, PICOLIBSDK_DIR, SDL2, testy, examples
  cmake/picopad.cmake            # picopad_sdk_objects(), picopad_add_game(), parse Makefile
  cmake/parse_makefile.cmake     # čtení SRC/CSRC z Makefile hry
  include/plat_api.h             # hranice host <-> platform (čisté C)
  host/host_prelude.h            # -include: guardy + host verze HW hlaviček
  host/hw/host_cpu.h             # náhrada sdk_cpu.h
  host/hw/host_timer.h           # náhrada sdk_timer.h
  host/hw/host_spinlock.h        # náhrada sdk_spinlock.h
  host/hw/host_gpio.h            # náhrada sdk_gpio.h
  host/hw/host_pwm.h             # náhrada sdk_pwm.h
  host/hw/host_irq.h             # náhrada sdk_irq.h
  host/hw/host_adc.h             # náhrada sdk_adc.h
  host/hw/host_dma.h             # náhrada sdk_dma.h
  host/clock.c                   # reálné/virtuální hodiny, Time/Time64/WaitUs/WaitMs
  host/pump.c                    # pumpa SysTicku, IRQ mutex
  host/runtime.c                 # RuntimeInit, ROM funkce, CurrentFreq, RoscRand, ResetToBootLoader, panic, host_run_game
  host/sdk_stubs.c               # zbytek _sdk/src API (USB stuby, Flash*, Spin*, Watchdog, …) potřebný k linku
  host/disp.c                    # FrameBuf, dirty rect, DispInit/DispUpdate/…, model přenosu
  host/keys.c                    # GPIO_In pro piny tlačítek
  host/audio.c                   # SetHandler/PWM_* záchyt, host_audio_render
  host/sd.c                      # lib_sd API: "karta není vložena" (obraz disku je milník 3)
  host/bat.c                     # BatInit/BatTerm/GetBat*/… pevné hodnoty
  host/heap.S                    # __malloc_start__/__malloc_end__ pro lib_malloc.c
  platform/main.cpp              # CLI, okno, smyčka událostí, herní vlákno, audio
  platform/keymap.c / keymap.h   # mapování SDL scancode -> bit tlačítka (testovatelné)
  platform/cli.c / cli.h         # parsování argumentů (testovatelné)
  tests/check.h                  # mini assert framework
  tests/plat_fake.c              # testovací implementace plat_api.h
  tests/config/config.h          # konfigurace SDK pro testy
  tests/test_*.c                 # unit testy host vrstvy
  examples/HELLO/                # minimální hra mimo strom SDK
  tools/build_device.ps1         # ARM build hry mimo strom SDK bez zápisu do SDK
  tools/uf2check.py              # kontrola hlavičky PPAD + CRC v UF2
  deps/                          # stažené SDL2 (v .gitignore)
```

## Rozhraní host ↔ platform (`include/plat_api.h`)

Používá ho Task 3 až 7; žádná jiná jména se nevymýšlí.

```c
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

typedef struct {
    int virtual_clock;          /* 0 = real time, 1 = virtual time */
    int fast_display;           /* 1 = no SPI transfer cost */
    unsigned int seed;          /* RoscRand seed */
    int audio_rate;             /* Hz, 0 = no audio (mixer not driven) */
} host_config;

/* implemented by platform/ (or tests/plat_fake.c), called by host/ */
unsigned long long plat_now_us(void);                 /* monotonic real time */
void plat_sleep_us(unsigned long long us);
void plat_present(const unsigned short* fb, int x, int y, int w, int h); /* copy rect of the 320-wide RGB565 buffer */
unsigned int plat_key_bits(void);
void plat_log(const char* msg);

/* implemented by host/, called by platform/ */
void host_configure(const host_config* cfg);
int  host_run_game(void);                 /* RuntimeInit + game_main on the calling thread; returns exit code */
void host_audio_render(short* out, int frames);   /* mono s16; called from audio thread */
unsigned long long host_now_us(void);     /* emulated device time (µs since start) */

#ifdef __cplusplus
}
#endif
#endif
```

---

### Task 1: Toolchain a kostra projektu

**Files:**
- Create: `CMakeLists.txt`, `.gitignore`, `tests/check.h`, `tests/test_toolchain.c`, `README.md`

**Interfaces:**
- Produces: CMake proměnné `PICOLIBSDK_DIR`, `SDL2_DIR` (do `deps/SDL2/i686-w64-mingw32`), cíl `SDL2::SDL2`; `tests/check.h` s makry `CHECK(cond)` a `CHECK_EQ(a,b)` (vypíše soubor:řádek a hodnoty, nastaví globální `check_failures`) a `CHECK_DONE()` (vrátí `check_failures ? 1 : 0`).

- [ ] **Step 1: Se souhlasem uživatele nainstalovat nástroje**

```powershell
winget install --id BrechtSanders.WinLibs.POSIX.UCRT --architecture x86
winget install --id Ninja-build.Ninja
```

Pokud winget i686 variantu WinLibs nenabízí, stáhnout i686 UCRT zip z https://winlibs.com do `C:\Progs\winlibs32` (opět se souhlasem). SDL2: stáhnout poslední `SDL2-devel-2.x.y-mingw.tar.gz` z https://github.com/libsdl-org/SDL/releases a rozbalit do `deps/SDL2`.

- [ ] **Step 2: Ověřit překladač**

Run: `i686-w64-mingw32-gcc --version` (nebo `gcc --version` z WinLibs i686) a `gcc -dumpmachine`
Expected: `i686-w64-mingw32`

- [ ] **Step 3: Napsat `tests/test_toolchain.c`** – `main` volá `SDL_Init(0)`, `CHECK_EQ(SDL_Init(0), 0)`, `CHECK_EQ(sizeof(void*), 4)`, `SDL_Quit()`, vrací `CHECK_DONE()`.

- [ ] **Step 4: `CMakeLists.txt`** – projekt C/CXX, `enable_testing()`, najít SDL2 v `deps/SDL2/i686-w64-mingw32`, cíl `test_toolchain` + `add_test`. Post-build kopie `SDL2.dll` vedle exe. `.gitignore`: `build/`, `deps/`.

- [ ] **Step 5: Spustit**

Run: `cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build && ctest --test-dir build -R toolchain --output-on-failure`
Expected: `100% tests passed`

- [ ] **Step 6: `README.md`** – postup instalace a buildu z kroků 1–5.

- [ ] **Step 7: Commit** `git commit -m "build: i686 MinGW + SDL2 toolchain skeleton"`

---

### Task 2: Překlad přenositelných knihoven SDK pro host

**Files:**
- Create: `host/host_prelude.h`, `host/hw/host_{cpu,timer,spinlock,gpio,pwm,irq,adc,dma}.h`, `cmake/picopad.cmake`, `tests/config/config.h`, `tests/config/include.h`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: CMake funkce `picopad_sdk_objects(<target> <config_dir>)` → OBJECT knihovna s přenositelnými zdroji SDK přeloženými s `-include host/host_prelude.h`, include cestami `<config_dir>` a `PICOLIBSDK_DIR` a definicemi z Global Constraints. Seznam zdrojů (relativně k SDK): `_font/font.c`; `_lib/src/lib_{calendar,calendar64,color,crc,decnum,draw,escpkt,fat,list,mat2d,print,pwmsnd,rand,rect,stream,text,textlist,textnum,tprint,tree,alarm,event,miniring,ring,malloc}.c`; `_devices/picopad/picopad_{init,key,led,ss}.c`.
- Produces: `host_prelude.h` předdefinuje `_SDK_CPU_H _SDK_TIMER_H _SDK_SPINLOCK_H _SDK_GPIO_H _SDK_PWM_H _SDK_IRQ_H _SDK_ADC_H _SDK_DMA_H` a includuje `host/hw/host_*.h`. Každá `host_*.h` deklaruje vše, co z originální hlavičky používají ostatní hlavičky a kompilované soubory (stejná jména, konstanty a makra, např. `IRQ_LOCK`, `IRQ_UNLOCK`, `CORE_NUM`, `IRQ_PWM_WRAP`, `PWM_GPIOTOSLICE`, `PWM_GPIOTOCHAN`, `dmb()`, `CpuID()`), přičemž inline těla nesahají na registry.
- Produces (deklarace, implementace v Task 3–6): `void host_pump(void);` `void host_irq_lock(void);` `void host_irq_unlock(void);` `u32 host_time_us32(void);` `u8 host_gpio_in(int pin);` `void host_pwm_comp(int slice, int chan, u32 val);` `void host_pwm_enable(int slice, int on);` `void host_set_handler(int irq, void (*fn)(void));`
- Konkrétní mapování: `dmb/dsb/isb` = `__sync_synchronize(); host_pump();` (pumpa sama ignoruje volání z jiného než herního vlákna), `LockIRQ/UnlockIRQ` = `host_irq_lock/unlock`, `CpuID()` = 0, `Time()` = `host_time_us32()`, `TimeMs()` = `(u16)(Time()>>10)` jako originál, `GPIO_In(pin)` = `host_gpio_in(pin)`, ostatní GPIO/ADC/DMA = no-op/0, `PWM_Comp` = `host_pwm_comp`, `PWM_Enable/PWM_Disable` = `host_pwm_enable`, `SetHandler` = `host_set_handler`, `NVIC_*` = no-op. `Endian/Swap/Ror/Rol/Reverse*` přes `__builtin_bswap32` a posuny.

- [ ] **Step 1: Test – cíl `sdk_objs_test`** (`picopad_sdk_objects(sdk_objs_test tests/config)`), `tests/config/config.h` obsahuje jen `#include CONFIG_DEF_H` (a `#define USE_USBPAD 1` jako TETRIS, aby se pokryla USB cesta), `tests/config/include.h` = `#include INCLUDES_H`.

- [ ] **Step 2: Spustit** `cmake --build build --target sdk_objs_test` – Expected: FAIL (chybí host hlavičky / registrové inline).

- [ ] **Step 3: Implementovat `host_prelude.h` a `host/hw/host_*.h`** podle Interfaces. Postup: překládat, číst chyby, doplňovat chybějící deklarace z originální hlavičky (kopírovat deklarace a konstanty, ne těla s registry). Kolize typedefů MinGW (`uintptr_t`, `int32_t`) řešit definicemi v `picopad_sdk_objects` (např. `_UINTPTR_T_DEFINED`, `_INTPTR_T_DEFINED`), ne úpravou SDK.

- [ ] **Step 4: Spustit** `cmake --build build --target sdk_objs_test` – Expected: všechny objekty přeložené bez chyb (varování povolena).

- [ ] **Step 5: Commit** `git commit -m "host: compile portable PicoLibSDK libraries with host HW headers"`

---

### Task 3: Hodiny, pumpa SysTicku, runtime a stuby SDK

**Files:**
- Create: `host/host_internal.h`, `host/clock.c`, `host/pump.c`, `host/runtime.c`, `host/sdk_stubs.c`, `host/sd.c`, `host/bat.c`, `host/heap.S`, `tests/plat_fake.c`, `tests/test_clock.c`, `tests/test_rom.c`
- Modify: `cmake/picopad.cmake` (přidat host zdroje do `picopad_sdk_objects`), `CMakeLists.txt`

**Interfaces:**
- Consumes: Task 2 deklarace `host_*`; `plat_api.h`.
- Produces: `host/host_internal.h` (jen pro `host/`): `void host_wait_us(unsigned long long us);` (čekání v emulovaném čase podle režimu hodin, s pumpou), `const host_config* host_cfg(void);`, `int host_is_game_thread(void);`.
- Produces: `host_configure`, `host_run_game`, `host_now_us`, `host_pump`, `host_irq_lock/unlock`, `host_time_us32`; SDK funkce `Time64`, `WaitUs`, `WaitMs`, `RuntimeInit`, `ResetToBootLoader`, `panic`, `RoscRand`, `RoscRand32`, ROM ukazatele `popcount reverse clz ctz MemSet MemSet32 MemCopy MemCopy32`, `CurrentFreq[]`, `SysTime UnixTime CurTimeMs`, USB stuby (`UsbHostInit UsbTerm UsbKeyIsMounted=False UsbKeyIsPressed=False UsbGetKeyRel=0 UsbGetChar=0 UsbFlushKey UsbHostIsInit=False`, každý volá `host_pump()`), `lib_sd` API (`SDType=0`, `SDConnect` vrací False, `SDInit/SDTerm/SDDisconnect` no-op, `SDMediaSize=0`, `SDReadSect/SDWriteSect` vrací False), baterie (`BatInit/BatTerm` no-op, `GetBat` 4.2f, `GetBatInt` 4200). Heap pro `lib_malloc.c`: `host/heap.S` definuje `___malloc_start__` (zarovnání 8) a za 64 KiB `___malloc_end__` (i686 přidává podtržítko). Funkce, které `lib_fat.c` čte z flash (`CheckApp` apod.), musí vrátit "není aplikace" – zajistit tak, aby jejich vstupní data (`XIP_BASE+BOOTLOADER_SIZE`) nebyla dereferencována; pokud to bez změny SDK nejde, vyřadit je z linku nahrazením symbolů ve `sdk_stubs.c` (rozhodne implementátor, zdokumentuje v komentáři).
- Chování hodin: reálný režim `host_now_us = plat_now_us() - start`; virtuální režim čítač posouvaný jen `WaitUs`/`WaitMs`/modelem displeje. `WaitUs(us)`: reálný režim spí po kouscích ≤ 1 ms a mezi nimi volá `host_pump()`; virtuální posune čas a zavolá `host_pump()`.
- Pumpa: běží jen na herním vlákně (thread-local příznak nastavený v `host_run_game`); spustí `SysTick_Handler`-ekvivalent tolikrát, kolik 5ms period od posledního ticku uplynulo podle `host_now_us()`: `SysTime += 5`, `CurTimeMs/UnixTime` jako originál, `AlarmUpdate()`, `KeyScan()`. Reentrance zakázaná (příznak). V reálném režimu: pokud byla pumpa volána víc než 1000× bez uplynutí 1 ms, zavolá `plat_sleep_us(200)` (ochrana jádra v aktivním čekání).
- `host_run_game`: nastaví herní vlákno, `setjmp`; zavolá `RuntimeInit()` (ROM ukazatele, `CurrentFreq[CLK_SYS] = 2048*audio_rate` nebo `2048*44100` když audio_rate=0, `RandInit()` + seed přes `RoscRand`, `AlarmInit`, `MemInit`, `DeviceInit()`), pak `game_main()`. `ResetToBootLoader()` = `DeviceTerm()` + `longjmp` s kódem 0; `panic`/fatální chyba = `plat_log` + `longjmp` s kódem 1.

- [ ] **Step 1: `tests/plat_fake.c`** – implementace `plat_api.h`: `plat_now_us` vrací proměnnou `fake_now` (test ji posouvá), `plat_sleep_us` přičte k `fake_now`, `plat_present` zaznamená poslední volání a kopii obdélníku do `fake_fb[240][320]`, `plat_key_bits` vrací `fake_keys`, `plat_log` uloží poslední zprávu.

- [ ] **Step 2: Napsat `tests/test_clock.c`** (hra `game_main` je definovaná v testu a vrací po kontrolách):

```c
/* virtual clock: WaitMs advances time exactly and runs SysTick every 5 ms */
host_config cfg = { .virtual_clock = 1, .fast_display = 1, .seed = 1, .audio_rate = 0 };
/* in game_main: */
u32 t0 = Time(); u32 s0 = SysTime;
WaitMs(50);
CHECK_EQ(Time() - t0, 50000);
CHECK_EQ(SysTime - s0, 50);
WaitUs(4999); CHECK_EQ(SysTime - s0, 50);   /* not yet next tick */
WaitUs(1);    CHECK_EQ(SysTime - s0, 55);
ResetToBootLoader();                         /* host_run_game returns 0 */
/* in main: */ CHECK_EQ(host_run_game(), 0);
```

Navíc `test_busy_wait`: hra volá 2000× `dmb()` v reálném režimu nad `plat_fake`; `CHECK(fake_now posunut o >= 200)` (ochrana jádra spí).

- [ ] **Step 3: Napsat `tests/test_rom.c`** – po `RuntimeInit()`: `CHECK_EQ(clz(1), 31)`, `CHECK_EQ(ctz(8), 3)`, `CHECK_EQ(popcount(0xF0F0), 8)`, `CHECK_EQ(reverse(1), 0x80000000)`, `MemCopy`/`MemSet` přes malé pole; `RandU8Max(9) <= 9` pro 100 hodnot; dvě spuštění se stejným `seed` dají stejnou posloupnost `RandU32()` (10 hodnot).

- [ ] **Step 4: Spustit** `ctest --test-dir build -R "clock|rom" --output-on-failure` – Expected: FAIL (link errors).

- [ ] **Step 5: Implementovat** `clock.c`, `pump.c`, `runtime.c`, `sdk_stubs.c`, `sd.c`, `bat.c`, `heap.S` podle Interfaces; doplňovat stuby, dokud test nelinkuje.

- [ ] **Step 6: Spustit** `ctest --test-dir build -R "clock|rom" --output-on-failure` – Expected: PASS

- [ ] **Step 7: Commit** `git commit -m "host: clock, SysTick pump, runtime init and SDK stubs"`

---

### Task 4: Displej

**Files:**
- Create: `host/disp.c`, `tests/test_display.c`

**Interfaces:**
- Consumes: `plat_present`, `host_now_us`, `host_wait_us`, `host_cfg` z Task 3.
- Produces: celé API z `_display/st7789/st7789.h` použité kompilovaným kódem: `FrameBuf` (`ALIGNED u16 FrameBuf[FRAMESIZE]`), dirty rect funkce převzaté z `st7789.c` (stejné chování), `DispInit(u8 rot)`, `DispTerm`, `DispUpdate`, `DispUpdateAll`, `DispAutoUpdate(u32 ms)`, `DispBacklight`, `DispBacklightUpdate`, a ostatní symboly ze `st7789.h`, které linker vyžádá (no-op nebo ekvivalent).
- `DispUpdate`: pokud dirty rect neprázdný → `plat_present(FrameBuf, x, y, w, h)`, pak (není-li `fast_display`) čekání `w*h*16/24` µs, pak `DispDirtyNone()`; vždy `host_pump()`.

- [ ] **Step 1: Napsat `tests/test_display.c`** (virtuální hodiny):

```c
DrawClear();                       /* marks full screen dirty */
DrawRect(10, 20, 30, 40, COL_RED);
u32 t0 = Time();
DispUpdate();
CHECK_EQ(fake_present_w, 320); CHECK_EQ(fake_present_h, 240);
CHECK_EQ(fake_fb[20][10], COL_RED); CHECK_EQ(fake_fb[59][39], COL_RED); CHECK_EQ(fake_fb[60][40], COL_BLACK);
CHECK_EQ(Time() - t0, 51200);      /* full-screen SPI transfer at 24 MHz */
DrawPoint(319, 239, COL_WHITE);    /* edge rect */
DispUpdate();
CHECK_EQ(fake_present_x, 319); CHECK_EQ(fake_present_y, 239); CHECK_EQ(fake_fb[239][319], COL_WHITE);
DispUpdate();                      /* nothing dirty -> no present, no time */
```

Druhý test s `fast_display = 1`: `Time()` se při `DispUpdate` neposune.

- [ ] **Step 2: Spustit** `ctest -R display` – Expected: FAIL
- [ ] **Step 3: Implementovat `host/disp.c`**
- [ ] **Step 4: Spustit** `ctest -R display` – Expected: PASS
- [ ] **Step 5: Commit** `git commit -m "host: display framebuffer, dirty rect and SPI transfer model"`

---

### Task 5: Klávesy

**Files:**
- Create: `host/keys.c`, `platform/keymap.h`, `platform/keymap.c`, `tests/test_keys.c`, `tests/test_keymap.c`

**Interfaces:**
- Consumes: `plat_key_bits()`.
- Produces: `u8 host_gpio_in(int pin)` – pro piny `BTN_UP_PIN..BTN_B_PIN` (picopad10: UP 4, LEFT 3, RIGHT 2, DOWN 5, X 9, Y 8, A 7, B 6) vrací 0 když je odpovídající bit `plat_key_bits()` nastaven (aktivní v nule), jinak 1; ostatní piny 1; volá `host_pump()`.
- Produces: `typedef enum { KEYMAP_LETTERS = 0, KEYMAP_LAYOUT = 1 } keymap_mode;` a `unsigned int keymap_bit(keymap_mode mode, int sdl_scancode);` (vrací `PLAT_KEY_BIT(code)` nebo 0; nezávislé na SDL hlavičkách – scancody jako `int`, hodnoty SDL2: A=4, B=5, S=22, X=27, Y=28, Z=29, ESC=41, RIGHT=79, LEFT=80, DOWN=81, UP=82).
- "Ťuknutí" (Review Focus 4): platformní vrstva drží bit stisku nastavený, dokud neproběhl aspoň jeden tick po stisku (`host_now_us()` posun ≥ 5000 µs od stisku), i když klávesa byla mezitím puštěna.

- [ ] **Step 1: `tests/test_keymap.c`**

```c
CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 4),  PLAT_KEY_BIT(7));  /* A -> KEY_A */
CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 28), PLAT_KEY_BIT(6));  /* Y -> KEY_Y */
CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 41), PLAT_KEY_BIT(6));  /* Esc -> KEY_Y */
CHECK_EQ(keymap_bit(KEYMAP_LETTERS, 29), 0);                /* Z unmapped */
CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 4),   PLAT_KEY_BIT(5));  /* A -> KEY_X */
CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 22),  PLAT_KEY_BIT(6));  /* S -> KEY_Y */
CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 29),  PLAT_KEY_BIT(7));  /* Z -> KEY_A */
CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 27),  PLAT_KEY_BIT(8));  /* X -> KEY_B */
CHECK_EQ(keymap_bit(KEYMAP_LAYOUT, 82),  PLAT_KEY_BIT(1));  /* Up -> KEY_UP */
```

- [ ] **Step 2: `tests/test_keys.c`** (virtuální hodiny, `plat_fake`):

```c
fake_keys = PLAT_KEY_BIT(KEY_A) | PLAT_KEY_BIT(KEY_LEFT);
WaitMs(5);
CHECK(KeyPressed(KEY_A)); CHECK(KeyPressed(KEY_LEFT)); CHECK(!KeyPressed(KEY_B));
u8 k1 = KeyGet(), k2 = KeyGet();
CHECK((k1 == KEY_A && k2 == KEY_LEFT) || (k1 == KEY_LEFT && k2 == KEY_A));
fake_keys = 0; WaitMs(100);
CHECK(!KeyPressed(KEY_A));
```

- [ ] **Step 3: Spustit** `ctest -R key` – Expected: FAIL
- [ ] **Step 4: Implementovat** `host/keys.c`, `platform/keymap.c`
- [ ] **Step 5: Spustit** `ctest -R key` – Expected: PASS
- [ ] **Step 6: Commit** `git commit -m "host: keys via GPIO shim, keymaps letters/layout"`

---

### Task 6: Zvuk

**Files:**
- Create: `host/audio.c`, `tests/test_audio.c`

**Interfaces:**
- Consumes: `host_set_handler`, `host_pwm_comp`, `host_pwm_enable` deklarace z Task 2; IRQ mutex z Task 3.
- Produces: implementace `host_set_handler` (tabulka podle čísla IRQ), `host_pwm_comp` (uloží poslední hodnotu kanálu), `host_pwm_enable`, `host_audio_render(short* out, int frames)`: pod `host_irq_lock()` pro každý vzorek zavolá uložený handler `IRQ_PWM_WRAP` (je-li PWM povoleno) a zapíše `(samp-1024)*32`; bez handleru nebo před `PWM_Enable` zapíše 0.

- [ ] **Step 1: `tests/test_audio.c`** (`audio_rate = 44100`):

```c
static const u8 snd[4410];          /* filled with 0xC0 in test setup */
short buf[512];
host_audio_render(buf, 512);
for (i = 0; i < 512; i++) CHECK_EQ(buf[i], 0);          /* silence before any sound */
PlaySound(snd, sizeof(snd));
host_audio_render(buf, 512);
CHECK(buf[100] > 0);                                      /* 0xC0 > 0x80 midpoint */
host_audio_render(buf, 512); /* … render >= 4410*2 frames total … */
CHECK(!PlayingSound());                                   /* 4410 samples @22050 Hz = 0.2 s = 8820 frames @44100 */
```

Souběh (Review Focus 3): vlákno volá 10 000× `host_audio_render(buf, 64)`, zatímco herní vlákno 1000× volá `PlaySound(snd, 64)`; test projde bez pádu a na konci `PlayingSound()` do 10 dalších renderů skončí.

- [ ] **Step 2: Spustit** `ctest -R audio` – Expected: FAIL
- [ ] **Step 3: Implementovat `host/audio.c`**
- [ ] **Step 4: Spustit** `ctest -R audio` – Expected: PASS
- [ ] **Step 5: Commit** `git commit -m "host: PWM sound capture and audio render"`

---

### Task 7: Platforma, CLI a TETRIS

**Files:**
- Create: `platform/main.cpp`, `platform/cli.h`, `platform/cli.c`, `cmake/parse_makefile.cmake`, `tests/test_cli.c`, `tests/parse_makefile_test.cmake`
- Modify: `cmake/picopad.cmake` (`picopad_add_game`), `CMakeLists.txt` (`picopad_add_game(TETRIS ${PICOLIBSDK_DIR}/PicoPad/GAME/TETRIS)`)

**Interfaces:**
- Consumes: `plat_api.h`, `keymap_bit`, vše z `host/`.
- Produces: `typedef struct { int scale; keymap_mode keys; int fast_display; unsigned int seed; int seed_set; long long max_time_ms; } cli_options;` a `int cli_parse(int argc, char** argv, cli_options* out, char* err, int errlen);` (0 = OK, jinak chyba → exit 2). Výchozí: scale 3, keys letters, fast_display 0, seed z času, max_time 0 (bez limitu). Formát `--max-time` akceptuje `2000`, `2000ms`, `2s`.
- Produces: CMake `picopad_add_game(<NAME> <GAME_DIR>)`: přečte `<GAME_DIR>/Makefile`, posbírá cesty z řádků `SRC +=`, `CSRC +=` (relativní k `<GAME_DIR>`), přeloží je s `-Dmain=game_main` proti `picopad_sdk_objects(<NAME>_sdk <GAME_DIR>)`, slinkuje s `platform/` a SDL2 do `<NAME>.exe`, zkopíruje `SDL2.dll`. Nikam do `<GAME_DIR>` ani do SDK nezapisuje.
- `main.cpp`: SDL init (video, audio, events), okno `320*scale x 240*scale` s titulkem jména hry, textura RGB565, herní vlákno volá `host_run_game()`; hlavní vlákno: události (klávesy → bitová maska přes `keymap_bit`, ťuknutí dle Task 5), prezentace posledního předního bufferu (mutex), zavření okna/Ctrl+C → ukončení procesu s kódem 0 do 1 s, `--max-time` → kód 3, konec herního vlákna → jeho kód. Audio: `SDL_OpenAudioDevice` mono s16 44100 Hz, callback volá `host_audio_render`.

- [ ] **Step 1: `tests/test_cli.c`** – `--scale 2 --keys layout --fast-display --seed 7 --max-time 2s` → scale 2, `KEYMAP_LAYOUT`, fast 1, seed 7, max 2000; bez argumentů výchozí hodnoty; `--keys foo` → návrat ≠ 0 a `err` obsahuje `--keys`.
- [ ] **Step 2: `tests/parse_makefile_test.cmake`** – nad kopií Makefile TETRISu (SRC: `src/main.cpp img/tiles.cpp snd/brick.cpp snd/gameover.cpp snd/levelup.cpp snd/line.cpp`) vrátí přesně těchto 6 souborů; spouští se `cmake -P` přes `add_test`.
- [ ] **Step 3: Spustit** `ctest -R "cli|makefile"` – Expected: FAIL
- [ ] **Step 4: Implementovat** `cli.c`, `parse_makefile.cmake`, `picopad_add_game`, `main.cpp`
- [ ] **Step 5: Spustit** `ctest -R "cli|makefile"` – Expected: PASS
- [ ] **Step 6: Smoke test TETRIS** – `add_test(tetris_smoke TETRIS.exe --max-time 2s --fast-display)` s `WILL_FAIL` nahrazeným kontrolou návratového kódu 3 (např. přes CMake skript, který exe spustí a ověří `3`). Run: `ctest -R tetris_smoke` – Expected: PASS (běží 2 s bez pádu).
- [ ] **Step 7: Ruční kontrola uživatelem** – `build\TETRIS.exe`: titulní obrazovka jako na zařízení, hra jde ovládat, zvuky hrají, Y vrátí kód 0 a zavře okno, zavření okna křížkem ukončí do 1 s. Výsledek zapsat do README (sekce "Ověřeno").
- [ ] **Step 8: Commit** `git commit -m "platform: SDL window, audio, CLI; TETRIS runs"`

---

### Task 8: Ukázková hra mimo strom SDK

**Files:**
- Create: `examples/HELLO/{Makefile,config.h,include.h,src/main.cpp,src/main.h}`, `tools/build_device.ps1`, `tools/uf2check.py`
- Modify: `CMakeLists.txt` (`picopad_add_game(HELLO ${CMAKE_SOURCE_DIR}/examples/HELLO)` + testy)

**Interfaces:**
- Consumes: `picopad_add_game`.
- Produces: `tools/build_device.ps1 -GameDir <dir> -Device picopad10` – v `<dir>` spustí `<SDK>\_tools\make.exe all` s `PICO_ROOT_PATH=<SDK>` a `DEVICE/DEVCLASS` z `_setup.bat` logiky (picopad10 → `DEVICE=picopad10 DEVCLASS=picopad`), pak `<SDK>\_tools\PicoPadLoaderCrc\LoaderCrc.exe <T>.bin <T>.uf2`; do SDK stromu nic nekopíruje. ARM toolchain v PATH: `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin`. Pozor: v `cmd` na tomto stroji platí `NoDefaultCurrentDirectoryInExePath`, proto vše volat plnou cestou.
- Produces: `tools/uf2check.py <file.uf2>` – exit 0 když: všechny bloky mají UF2 magic a rodinu `0xE48BFF56`, aplikace na offsetu 0x8000 má na slovech 48/49/50/53 `0x44415050`, délku, CRC a `0x64617070`, a CRC (poly `0x04C11DB7`, init `0xFFFFFFFF`, MSB-first, bez xorout) přes `len` bajtů od offsetu 204 aplikace sedí. Spouštět `C:\Progs\anaconda3\python.exe -I`.
- HELLO: `Makefile` s `PICO_ROOT_PATH = ../../../PicoLibSDK` a `SRC += src/main.cpp`; `main()` vykreslí text "HELLO PICOPAD" a barevné obdélníky, `DispUpdate()`, čeká `KeyGet() == KEY_Y` a zavolá `ResetToBootLoader()`.

- [ ] **Step 1: Testy** – `add_test(hello_smoke HELLO.exe --max-time 1s --fast-display)` očekává kód 3; `add_test(hello_device …)` spustí `build_device.ps1` a pak `uf2check.py examples/HELLO/HELLO.uf2` (test označit labelem `device`, aby šel vynechat, když ARM toolchain chybí).
- [ ] **Step 2: Spustit** `ctest -R hello` – Expected: FAIL
- [ ] **Step 3: Implementovat** HELLO, `build_device.ps1`, `uf2check.py`
- [ ] **Step 4: Spustit** `ctest -R hello` – Expected: PASS; `git -C ..\PicoLibSDK status --short` – Expected: prázdný výstup (SDK beze změn).
- [ ] **Step 5: Ruční kontrola uživatelem** – `HELLO.uf2` nahrát přes BOOTSEL na Picopad: obraz odpovídá `HELLO.exe`, Y vrátí do loaderu.
- [ ] **Step 6: Commit** `git commit -m "examples: out-of-tree HELLO builds for emulator and device"`
