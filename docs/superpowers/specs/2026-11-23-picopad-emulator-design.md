# Picopad emulátor pro PC – návrh

Datum: 2026-11-23. Stav: návrh ke schválení. Přečteno nezávislým agentem v roli implementátora; jeho dvě otázky jsou zapracované (smyčky bez volání hostitele, umístění nových her).

## Proč

Uživatel chce nechávat agenty generovat nové hry pro Picopad (handheld na Raspberry Pi Pico, displej ST7789 320x240 RGB565, 8 tlačítek, PWM zvuk, SD karta). Každá iterace přes skutečné zařízení (build, BOOTSEL, ruční hraní) je pomalá a agent ji nemůže dělat sám. Emulátor na PC má zkrátit smyčku "napiš – přelož – vyzkoušej" na sekundy a umožnit agentovi hru bez člověka spustit, ovládat a zkontrolovat výsledné obrazovky. Druhotný cíl: zahrát si na PC hry, které už jsou v repu PicoLibSDK.

## Cíle

1. Hra napsaná proti PicoLibSDK (Panda381, v2.09) se ze **stejných zdrojáků beze změn** přeloží jak pro Picopad (stávající ARM build), tak jako nativní Windows .exe, které běží v okně.
2. Kreslení vypadá na pixel stejně jako na zařízení – používá se přímo kreslicí knihovna SDK, ne její reimplementace.
3. **Interaktivní režim:** okno se zvětšeným displejem, ovládání klávesnicí (a volitelně gamepadem), zvuk, tempo hry odpovídá zařízení.
4. **Agentní (headless) režim:** bez okna a zvuku, s virtuálním časem, vstupy ze skriptu, ukládáním snímků obrazovky do PNG v zadaných časech a deterministickým během (stejný skript + seed = stejné snímky).
5. Spustit lze i existující hry z repa; první milník je PicoPad/GAME/TETRIS, pak všechny hry z PicoPad/GAME a PicoPad/TINYGAME.

## Necíle

- Emulace procesoru RP2040 / spouštění UF2 souborů (to je samostatný případný budoucí projekt, varianta C níže).
- Programy, které sahají přímo na hardware: EMU/NES, EMU/GBC, EMU/PC, EMU/141PF, dema RTXFAST, RAYTRACE, FOUNTAIN, LEVMETER, HSTXDEMO.
- Varianty Picopadu s jiným výstupem (VGA, HSTX/HDMI), Picoino, Picotron. Emuluje se konfigurace picopad10; Picopad 2.0 má stejné API, takže hry pro něj jsou pokryté také.
- USB (zařízení i host), baterie (vrací pevnou hodnotu), LED (stačí no-op, případně indikátor v okně), PSRAM, druhé jádro.
- Přesná emulace rychlosti CPU. Hry, které se spoléhají na pomalost procesoru místo `WaitMs`/`Time`, poběží rychleji.
- Linux/macOS. Návrh je pro Windows; přenositelnost nesmí být zbytečně zablokovaná, ale neřeší se (pozor: SDK má `u32 = unsigned long`, na 64bit Linuxu neplatí `sizeof(u32)==4`).
- Pipeline generování her agenty – je to navazující projekt, který emulátor využije.

## Anti-cíle (čím by to selhalo, i kdyby to technicky fungovalo)

- Úpravy zdrojáků PicoLibSDK nebo her kvůli emulátoru. SDK zůstává čistý upstream klon; žádné `#ifdef EMULATOR` ve hrách.
- Hra v emulátoru vypadá nebo se chová jinak než na zařízení (jiné vykreslení, jiné pořadí událostí kláves, výrazně jiné tempo).
- Headless režim, který je nedeterministický nebo se zasekne na smyčce typu `while (KeyGet() == NOKEY) {}`.

## Omezení a fakta ze SDK

- SDK leží v D:\Work\Prog\Picopad\PicoLibSDK. Cesta se do emulátoru předává proměnnou (CMake `PICOLIBSDK_DIR`).
- Všechny include v SDK jsou relativní (`"../../_sdk/inc/sdk_cpu.h"`), takže hlavičky nejde zastínit pořadím `-I`. Každá hlavička má ale include guard (`#ifndef _SDK_CPU_H`), takže ji lze vypnout předdefinováním guardu.
- Hardware se v SDK používá přes inline funkce v `_sdk/inc/*.h` (např. `Time()` čte `timer_hw->timerawl`, `GPIO_In` čte SIO registr, `dmb()` a `IRQ_LOCK` jsou ARM asm), přes ROM ukazatele funkcí (`clz`, `MemCopy`… plněné `RomFncInit`) a v souborech `_display/st7789/st7789.c`, `_lib/src/lib_sd.c`, `_devices/picopad/picopad_*.c` a `_sdk/src/*.c`.
- Přenositelný kód (`_lib/src/lib_draw.c`, fonty, text/print, FAT, mixer v `lib_pwmsnd.c`, `lib_rand.c`) neobsahuje asm ani přímé registry.
- Přenositelný kód ukládá ukazatele do `u32` (`sStream.cookie` na cestě `printf`/`DrawPrint`, ~35 míst v `lib_malloc.c`). Hry z repa volají `printf` 54×. Na 64bit by to padalo.
- TETRIS config.h nepodmíněně nastavuje `USE_USBPAD 1`, což vtahuje USB host stack (je potřeba stubovat `Usb*` funkce).
- Klávesy skenuje `KeyScan()` z přerušení SysTick každých 5 ms (`SYSTICK_KEYSCAN 1`); `KeyGet()` jen čte buffer.
- Zvuk: `PWMSndIrq()` v `lib_pwmsnd.c` je mixer, který při každém volání spočítá jeden vzorek a zapíše ho přes `PWM_Comp()` (rozsah 0..2047, 1024 = ticho). Krok je škálovaný `CurrentFreq[CLK_SYS]/2048` – na hostu se nastaví `CurrentFreq[CLK_SYS] = 2048 * vzorkovací_frekvence`.
- SD: FAT vrstva (`lib_fat.c`) mluví s kartou jen přes `SDConnect/SDDisconnect/SDMediaSize/SDReadSect/SDWriteSect/SDType` z `lib_sd.h`. Náhrada `lib_sd.c` za čtení/zápis sektorů z obrazu disku stačí.
- Displej: `FrameBuf` je `u16[320*240]` RGB565 (stejné bitové rozložení jako `SDL_PIXELFORMAT_RGB565`), kreslení označuje dirty obdélník, `DispUpdate()` ho odešle. Na zařízení trvá přenos celé obrazovky cca 51 ms (SPI 24 MHz, 16 bitů/px).
- Před `main()` běží `RuntimeInit()` (hodiny, ROM funkce, náhodný seed, SysTick, `DeviceInit()` = displej, klávesy, zvuk, SD). Hra definuje `int main()` bez parametrů a končí `ResetToBootLoader()` (Y).
- Nástroje na stroji: CMake 3.31 v C:\Progs\CMake\bin, Visual Studio 2026 Community, Arm GNU Toolchain 14.2 (pro zařízení). MinGW ani SDL zatím nejsou.

## Přístupy

- **A. Náhrada hardwarové vrstvy (zvoleno uživatelem).** Přenositelné knihovny SDK se přeloží beze změny pro PC; nahradí se jen vrstva, která sahá na čip (časovač, SysTick, displej, klávesy, výstup zvuku, SD, ROM funkce, USB stuby). Výhoda: věrné vykreslení, malý objem nového kódu (odhad 1500–3000 řádků), pokryje ~41 her z repa i nové hry. Nevýhoda: nepoběží programy sahající přímo na hardware.
- **B. Vlastní reimplementace API** (jako tools/tft-emulator v picopad-playground). Rychlý začátek, ale každá funkce se píše znovu a výsledek se od zařízení rozchází. Zamítnuto kvůli anti-cíli věrnosti.
- **C. Emulace RP2040** (rp2040js nebo vlastní jádro) spouštějící UF2. Pokryje vše včetně NES/GBC, ale vyžaduje modely ST7789 přes SPI+DMA, PWM, PIO, SD a dvou jader; několikanásobně větší. Odloženo jako možný samostatný projekt.

## Návrh

### Umístění a struktura

Nové repo D:\Work\Prog\Picopad\picopad-emu (git). PicoLibSDK se nemění. Navržená struktura (detaily na implementátorovi):

```
picopad-emu/
  CMakeLists.txt          # PICOLIBSDK_DIR, funkce picopad_add_game()
  host/                   # náhrada HW vrstvy (C, vidí jen hlavičky SDK)
    host_prelude.h        # vnucený -include: předdefinuje guardy a dává host verze hlaviček
    sdk_host.c            # časovač, SysTick, IRQ, ROM funkce, RuntimeInit, ResetToBootLoader…
    disp_host.c           # FrameBuf, DispInit/DispUpdate/dirty rect
    sd_host.c             # SD sektory z obrazu disku
    malloc_host.c, bat_host.c, usb_stub.c, …
  platform/               # SDL a Windows (C/C++, NEVIDÍ hlavičky SDK)
    main.cpp              # vstupní bod, CLI, okno, audio, skript, PNG
  tests/
  docs/
```

**Rozhodnutí (moje, k potvrzení):** dvě oddělené vrstvy překladových jednotek. Kód, který includuje `global.h` SDK, nesmí includovat SDL ani `windows.h` (kolize maker `MIN`, `MAX`, `B0..B31`, `PI`, `Bool`, `INLINE` a typedefů `int32_t`/`uintptr_t`). Komunikují přes malé rozhraní v čistém C (např. `plat_present_frame()`, `plat_key_state()`, `plat_now_us()`), které nepoužívá typy SDK.

### Překladač a platforma

- **32bit MinGW-w64 GCC (i686)** + **SDL2** (32bit mingw balíček) + CMake. GCC proto, že SDK je psané s rozšířeními GCC (atributy, `-funsigned-char`). 32bit proto, že přenositelný kód ukládá ukazatele do `u32`; na i686 to funguje beze změn SDK. Konkrétní distribuci (např. WinLibs i686 UCRT/MSVCRT přes winget nebo ruční stažení) a generátor (Ninja nebo MinGW Makefiles) zvolí implementátor.
- Příznaky pro kód SDK: `-std=gnu11` pro C (GCC 15 má výchozí C23, kde `typedef unsigned char bool` v global.h neprojde), `-std=gnu++17 -fno-exceptions -fno-rtti` pro C++, `-funsigned-char`, `-fno-strict-aliasing`, bez `-Werror`.
- Definice: `RP2040=1 USE_PICOPAD=1 USE_PICOPAD10=1 FLASHSIZE=0x200000 RAMSIZE=0x40000 INCLUDES_H="…/includes.h" CONFIG_DEF_H="…/config_def.h" USE_FLOATLIBC=1 USE_DOUBLELIBC=1`, volitelně `USE_CONFIG=0 USE_CANVAS=0 DRAW_HWINTER=0`, a ochrana proti kolizi typedefů MinGW (`_UINTPTR_T_DEFINED`, `_INTPTR_T_DEFINED` apod. – ověří implementátor).
- `main()` hry se přejmenuje při překladu `-Dmain=game_main` (jen pro zdrojáky hry), vlastní `main()` je v platform/.

### Které soubory SDK se překládají

- **Beze změny:** `_font/font.c`; z `_lib/src/`: lib_calendar, lib_calendar64, lib_color, lib_crc, lib_decnum, lib_draw, lib_escpkt, lib_fat, lib_list, lib_mat2d, lib_print, lib_pwmsnd, lib_rand, lib_rect, lib_stream, lib_text, lib_textlist, lib_textnum, lib_tprint, lib_tree, lib_alarm, lib_event, lib_miniring, lib_ring; z `_devices/picopad/`: picopad_init, picopad_key, picopad_led, picopad_ss. Soubory se překládají jednotlivě (ne přes unity soubory `sdk.c`/`lib.c`), seznam odpovídá ne-Windows větvi `_lib/_makefile.inc`.
- **Nahrazeno host verzí:** `_display/st7789/st7789.c` (zachovat jeho přenositelné části: definice `FrameBuf`, dirty-rect funkce), `_lib/src/lib_sd.c`, `_lib/src/lib_malloc.c` (pokud by na i686 nefungoval nad linker symboly heapu), `_lib/src/lib_config.c` (nebo `USE_CONFIG=0`), `_devices/picopad/picopad_bat.c`, všechny `_sdk/src/*.c` a `_sdk/usb_src/*.c`.
- **Hlavičky nahrazené přes host_prelude.h:** `sdk_cpu.h`, `sdk_timer.h`, `sdk_spinlock.h`, `sdk_gpio.h`, `sdk_pwm.h`, `sdk_irq.h`, `sdk_adc.h`, případně `sdk_dma.h`. Host verze musí zachovat všechna jména, konstanty a makra, která používají ostatní (originální) hlavičky. Ostatní hlavičky SDK zůstávají originální.

### Časování a "přerušení"

Hra běží na vlastním vlákně ("herní vlákno"); hlavní vlákno obsluhuje SDL (okno, události, prezentaci).

- **Hodiny** jsou abstrakce se dvěma implementacemi: reálný čas (QueryPerformanceCounter) v interaktivním režimu, virtuální čas v headless režimu. `Time()`, `Time64()`, `TimeMs()`, `WaitUs()`, `WaitMs()` jdou přes ni. `WaitMs` v reálném režimu spí, ve virtuálním posune virtuální čas.
- **SysTick (5 ms)** – obsluha (aktualizace `SysTime`/`UnixTime`, `AlarmUpdate()`, `KeyScan()`) se volá **na herním vlákně** z "pumpy", kterou volá každý vstup hry do hostitelské vrstvy (čtení času, čekání, `DispUpdate`, čtení GPIO, USB stuby, které `KeyGet` volá při každém průchodu díky `USE_USBPAD`). Pumpa spustí tolik ticků, kolik jich od minula uplynulo. Tím je pořadí událostí deterministické a odpadá souběh s kódem hry.
- **Ochrana proti zaseknutí ve virtuálním čase:** pokud hra volá pumpu opakovaně, aniž by virtuální čas postoupil (aktivní čekání na klávesu), pumpa po N voláních posune virtuální čas o jeden tick. Konkrétní N je na implementátorovi; podmínka je determinismus a to, že smyčka `while (KeyGet() == NOKEY) {}` ve virtuálním čase doběhne ke skriptovanému stisku.
- **Smyčky, které do hostitelské vrstvy vůbec nevolají** (v repu např. `while (PlayingSound()) {}` v MULTISET/PACMAN, SOKOBAN, VEGASLOT – `PlayingSoundChan` jen čte čítač, který snižuje mixer): pumpa se navíc volá i z `dmb()` a `GPIO_In()`, a pro zbytek existuje **pojistka proti zaseknutí** – hlídací vlákno, které po zadané reálné době (řádově 100 ms) bez jediného volání pumpy samo posune virtuální čas a spustí ticky a mixer (pod IRQ mutexem), a zapíše do logu varování `stall`. **Rozhodnutí (moje):** determinismus je zaručený pro hry, které hostitelskou vrstvu volají průběžně (to je drtivá většina a všechny generované hry mají být psané tak, aby to splňovaly); po zásahu pojistky je běh dokončený, ale determinismus už jen "best effort" a log to říká. Přednost má, aby se headless běh nezasekl.
- `LockIRQ/UnlockIRQ` (a tedy `IRQ_LOCK`) = globální rekurzivní mutex sdílený s audio vláknem. `dmb/dsb/isb` = paměťová bariéra kompilátoru/CPU. `CpuID()` = 0.

### Displej

- `DispUpdate()` zkopíruje dirty obdélník z `FrameBuf` do sdíleného "předního" bufferu (pod zámkem) a vynuluje dirty rect stejně jako originál. Hlavní vlákno předaný buffer nahraje do SDL textury RGB565 a vykreslí zvětšený (výchozí měřítko 3×, volba `--scale`).
- **Model rychlosti přenosu (výchozí zapnuto):** `DispUpdate` trvá tolik, kolik by na zařízení trval přenos dirty obdélníku po SPI 24 MHz (~51 ms na celou obrazovku); v reálném režimu se čeká, ve virtuálním posune čas. Lze vypnout (`--fast-display`). Důvod: hry jsou laděné na zařízení a jejich tempo s tímto časem počítá.
- `DispBacklight` ovlivní jas zobrazení (nice-to-have).

### Klávesy

- `GPIO_In(pin)` pro piny tlačítek vrací stav z bitové masky, kterou plní platformní vrstva (klávesnice/gamepad) nebo skript. `KeyScan()` z SDK tedy běží beze změny, včetně debounce a bufferu.
- Mapování kláves (rozhodnutí uživatele), přepínané volbou `--keys`:
  - `--keys=letters` (výchozí): šipky = D-pad, klávesy A, B, X, Y = stejnojmenná tlačítka Picopadu, Esc = Y. Důvod: když hra napíše "stiskni Y", uživatel zmáčkne Y.
  - `--keys=layout`: šipky = D-pad a čtverec kláves kopíruje fyzické rozložení Picopadu, Esc = Y.

    ```
    Picopad:  X Y      klávesnice:  A S
              A B                   Z X
    ```

    Tedy A = X, S = Y, Z = A, X = B. Pozor: rozložení Picopadu je zrcadlové vůči SNES (tam je A vpravo a B dole).
- Gamepad přes SDL GameController: D-pad/levá páčka, tlačítka podle fyzické polohy (nice-to-have). Další konfigurovatelnost mapování je nice-to-have.

### Zvuk

- `SetHandler(IRQ_PWM_WRAP, fn)` si handler uloží, `PWM_Comp` uloží poslední vzorek, `PWM_Enable` nastaví příznak "zvuk běží".
- Interaktivně: SDL audio callback (mono, 44100 Hz nebo co vyjde zařízení) pod globálním IRQ mutexem volá uložený handler jednou za výstupní vzorek a převádí `(samp-1024)*32` na s16. Před prvním `PWM_Enable` posílá ticho. `CurrentFreq[CLK_SYS]` se nastaví na `2048 * frekvence` před `DeviceInit`.
- Headless: mixer se volá podle virtuálního času na herním vlákně (pumpa), aby chování bylo stejné a deterministické; volitelně se vzorky zapíšou do WAV (`--wav`).

### SD karta

- `lib_sd.c` nahrazený čtením/zápisem 512B sektorů ze souboru s obrazem disku (`--sd cesta.img`); bez parametru karta "není vložena" (`SDConnect` selže).
- Pomocný příkaz pro vytvoření obrazu ze složky (např. přes samotný `lib_fat`: `DiskFormat` + kopírování souborů) je nice-to-have pro milník 3; volbu nástroje nechávám implementátorovi.
- Funkce `lib_fat.c`, které čtou aplikaci z flash (`CheckApp`, `GetHomePath`…), dostanou host data, která říkají "žádná aplikace/žádná cesta".

### Ostatní hardware

- ROM ukazatele (`popcount`, `reverse`, `clz`, `ctz`, `MemSet`, `MemSet32`, `MemCopy`, `MemCopy32`) se naplní C implementacemi v host `RuntimeInit`.
- `RoscRand()` vrací hodnoty z PRNG inicializovaného z `--seed` (výchozí: v interaktivním režimu čas, v headless 0), takže `RandInit` je deterministické.
- `ResetToBootLoader()` = `DeviceTerm()` + čisté ukončení (exit code 0). `panic`/fatální chyba = výpis + exit code ≠ 0.
- `DMA_CRC` softwarově (stejný výsledek jako hardware – je použité i v `lib_crc.c`).
- Baterie vrací pevnou plnou hodnotu, LED no-op.
- Host `RuntimeInit()` provede totéž co originál, pokud to dává smysl: ROM funkce, hodiny (`CurrentFreq`), `RandInit` + seed, `AlarmInit`, SysTick, `MemInit` (pokud se zachová `lib_malloc`), konstruktory C++ (u MinGW běží samy), `DeviceInit()`, pak `game_main()`.

### Sestavení her

- CMake funkce `picopad_add_game(<jméno> <adresář hry>)` přečte z `Makefile` hry řádky `SRC +=` / `CSRC +=` a vytvoří `<jméno>.exe` slinkované s host knihovnou. Include cesty: adresář hry (kvůli `config.h`/`include.h`) a kořen SDK. Tím jde přeložit libovolnou hru z repa bez psaní CMake pro každou zvlášť.
- **Hry mimo strom SDK (rozhodnutí uživatele):** nové, agenty generované hry budou žít ve vlastním repu D:\Work\Prog\Picopad\picopad-games vedle SDK. `picopad_add_game` proto nesmí předpokládat umístění hry tři úrovně pod kořenem SDK; cesty k SDK bere z `PICOLIBSDK_DIR`, nikdy relativně od hry. Pro zařízení build SDK hru mimo strom zvládne: `Makefile.inc` respektuje předem nastavené `PICO_ROOT_PATH` a `_c1.bat` se volá plnou cestou (ověřit v milníku 1; kopie UF2 do `!PicoPad10\<GRPDIR>` uvnitř SDK je vedlejší efekt, který lze ignorovat). Emulátor nesmí předpokládat `USE_USBPAD 1` – pumpa musí fungovat i bez USB stubů (přes `dmb()`, `GPIO_In()`, čas a pojistku proti zaseknutí).
- Samotné repo picopad-games, šablona hry a pipeline generování nejsou součástí tohoto projektu; tento projekt dodá jen ukázkovou minimální hru mimo strom SDK (adresář `examples/` v picopad-emu), která dokazuje, že stejné zdrojáky jdou přeložit pro zařízení i pro emulátor.
- Knihovna SDK se překládá per hra, protože `config.h` hry mění `USE_*` přepínače (prakticky: objektová knihovna per hra; optimalizace sdílení je na implementátorovi).

### Rozhraní příkazové řádky (každý .exe hry)

```
TETRIS.exe [--scale N] [--keys letters|layout] [--fast-display] [--sd card.img] [--seed N]
           [--headless] [--script input.txt] [--max-time 60s]
           [--shot-dir DIR] [--wav out.wav] [--trace-keys]
```

Formát skriptu (návrh, řádek = čas v ms od startu a příkaz):

```
500   press A
600   release A
2000  shot title          # uloží DIR/title.png (320x240, 1:1)
2000  hold RIGHT 300      # stisk na 300 ms
5000  shot game
9000  quit
```

Headless režim skončí na `quit`, na `--max-time` (exit code ≠ 0, aby agent poznal zaseknutí) nebo když hra skončí sama přes `ResetToBootLoader`. Na stdout jde krátký strojově čitelný log (čas, uložené snímky, důvod konce).

## Milníky

1. **TETRIS interaktivně:** přeloží se beze změn, běží v okně, ovládání klávesnicí, zvuk, Y ukončí. Kontrola očima proti zařízení. Navíc ukázková hra mimo strom SDK se přeloží pro emulátor i jako UF2 pro zařízení.
2. **Headless režim:** virtuální čas, skript, PNG, determinismus. Automatický test: skript pro TETRIS, dvě spuštění dají bajtově shodné PNG; referenční PNG uložené v tests/.
3. **Pokrytí repa a SD:** zkusit přeložit a spustit headless všechny hry z PicoPad/GAME a PicoPad/TINYGAME (snímek po pár sekundách), vytvořit přehled co funguje a co ne; podpora obrazu SD karty.

## Co je na implementátorovi

- Konkrétní distribuce MinGW i686 a SDL2 (i verze; SDL3 je přijatelná náhrada, pokud nic nebrání), generátor CMake, struktura souborů v host/ a platform/.
- Přesný mechanismus náhrady hlaviček (předdefinované guardy přes `-include`, nebo jiný způsob, který nemění SDK).
- Zda zachovat `lib_malloc.c` nad vlastním polem, nebo ho nahradit voláním libc.
- Detaily pumpy SysTicku a ochrany proti zaseknutí (při zachování determinismu).
- Formát logu headless režimu, přesná syntaxe skriptu (při zachování schopností výše).
- Testovací framework (stačí CTest se skripty a porovnáním PNG).
