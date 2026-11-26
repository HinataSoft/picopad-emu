# Picopad emulátor – milník 3 (pokrytí repa a SD karta) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Všech 41 her z `PicoPad/GAME` a `PicoPad/TINYGAME` se beze změn přeloží a headless spustí (včetně zvuku TINYGAME her), emulátor umí obraz SD karty (`--sd card.img`) a nástroj `picopad-sdimg` obraz vytvoří ze složky a přečte; přehled stavu her je v `docs/coverage.md`, generovaný skriptem.

**Architecture:** Host vrstva přestane vnucovat `USE_CONFIG=0` – `lib_config.c` se překládá beze změny, jen `ConfigLoad/ConfigSave/ConfigClear` se v jeho TU přejmenují (stejný trik jako `CheckApp` v `lib_fat.c`) a host dodá verze bez flash. Doplní se chybějící symboly (`random`, USB Mini-Port, `FlashSize`…). Zvuk TINYGAME her (bit-bang GPIO15 s `_delay_us(1)`) se vzorkuje z úrovně pinu podle emulovaného času. SD: `host/sd.c` čte a zapisuje sektory přes nové funkce `plat_sd_*` z `include/plat_api.h`, platforma je obslouží nad souborem obrazu. Nástroj `picopad-sdimg` používá přímo `lib_fat.c` ze SDK. Běh přes všechny hry obstarává CMake skript `tools/coverage/run.cmake` nad samostatným CMake projektem `tools/coverage/`.

**Tech Stack:** jako milník 2 – WinLibs GCC 16.2 i686, SDL2 2.32.10, CMake ≥ 3.21 + Ninja, CTest; Python (`C:\Progs\anaconda3\python.exe -I`) jen v testech. Nic nového se nestahuje.

**Spec:** docs/superpowers/specs/2026-11-23-picopad-emulator-design.md (milník 3; části SD karta, Ostatní hardware, Sestavení her). Předchozí plány: docs/superpowers/plans/2026-11-23-picopad-emu-m1.md, 2026-11-25-picopad-emu-m2.md.

**Výchozí stav (průzkum 2026-11-26, emulátor z main 6c5e8c5):** 19/41 her se přeloží a headless běží se smysluplným snímkem (všechny GAME kromě ANTS a MULTISET). Příčiny selhání: (A) 20 TINYGAME – `Config` nedeklarován, protože `picopad.cmake` vnucuje `USE_CONFIG=0`, (B) 3 TINYGAME (INVADERS, TBOMBER, TINYPAC) – `random()` v MinGW chybí, (C) 20 TINYGAME – link `PWMSndSlice`/`PWMSndChan` z `host/audio.c:53-54` při `USE_PWMSND 0`, (D) ANTS – `USE_USBPORT 1`, chybí `UsbPortInit/Term/Send/SendReady/Recv/Connected` a `UsbPort`, (E) MULTISET – chybí `FlashSize`, `FlashIDText`, `ADC_Temp()`. Zkušební build s obejitím A–C rozběhl všech 20 TINYGAME; AMANIA ukázala ve 3 s jen bílý obdélník (nevyšetřeno). SD kartu z her používá jen GAME/PICTOR (volitelný `/GAME/PICTOR.TOP`, bez karty běží).

## Global Constraints

- PicoLibSDK se nemění a build do jeho stromu nic nezapisuje; zdrojáky her se nemění, žádné `#ifdef EMULATOR`.
- Překladové jednotky s hlavičkami SDK (host/, testy host vrstvy, SDK část nástroje sdimg) nesmí includovat SDL, `windows.h`, `pthread.h` ani `<stdint.h>`; `include/plat_api.h` zůstává v čistém C s vestavěnými typy. Práce se soubory (obraz SD) je jen na platformní straně.
- `USE_CONFIG` už se nevnucuje (platí výchozí hodnota z `config_def.h` = 1, nebo `config.h` hry). `ConfigLoad()` na hostu = `ConfigSetDef(&Config)` (žádná trvalá konfigurace), `ConfigSave()` a `ConfigClear()` = nic (konfigurace žije jen v paměti). Výchozí `Config.volume = CONFIG_VOLUME_FULLSTEP`, takže hlasitost PWM zvuku zůstává jako v M2.
- Přímý zápis do flash (`FlashErase`, `FlashProgram` volané hrou) = fatální chyba (výpis na stderr, kód 1), ne tichý úspěch.
- Determinismus z M2 platí dál: v headless režimu nesmí reálný čas ovlivnit běh hry, snímky ani WAV. Referenční PNG z M2 (`tests/ref/*.png`) zůstávají bajtově stejné.
- Ukončovací kódy beze změny (0 quit/exit, 1 fatal, 2 argumenty/skript, 3 max-time); nově kód 2 i pro neotevřitelný nebo neplatný obraz SD.
- SD obraz: surový obraz disku (sektory 512 B od offsetu 0), velikost musí být násobek 512 a aspoň 64 KiB, jinak kód 2. Offsety 64bitové (obrazy do 4 GiB). Zápisy jdou do souboru (jako na kartu); při jakémkoli konci běhu (quit, exit, max-time, fatal) jsou zapsané zápisy uložené. Bez `--sd` karta „není vložena“ (chování M1). `--sd` platí v okně i v headless.
- `picopad-sdimg` je samostatné exe bez SDL; formátuje a zapisuje FAT výhradně přes `lib_fat.c` ze SDK, aby obraz byl to, co čte zařízení.
- Zvuk z GPIO (bit-bang): použije se jen pro pin `PWMSND_GPIO` (15) a jen dokud PWM zvuk SDK neběží (`PWM_Enable` na slice zvuku nebyl zavolán). Hodnota vzorku = podíl času v úrovni 1 během intervalu vzorku × 16384 jako s16 (klid v 0 = ticho 0, žádný stejnosměrný posun).

## Review Focus

1. **Neplatný `--sd`** (neexistuje, adresář, velikost není násobek 512, pod 64 KiB, jen pro čtení) – kód 2 se srozumitelnou hláškou při startu, ne pád ani tichá „karta není“. Soubor jen pro čtení: otevře se, zápisy selžou (`SDWriteSect` vrátí False). Test v Task 5.
2. **Hra zapíše na kartu a skončí** přes `ResetToBootLoader`, `quit` ve skriptu nebo `--max-time` – data jsou v souboru obrazu. Test v Task 5 (SDTEST + `picopad-sdimg get`).
3. **TINYGAME tón v reálném čase** – smyčka 100 000× `WaitUs(1)` (jako `Sound()` v PICOKIT) nesmí v okenním režimu trvat výrazně déle než 100 ms reálného času, jinak se tóny protáhnou a hry zpomalí. Test v Task 3 (limit 300 ms).
4. **Složka pro `picopad-sdimg create` se jménem, které `lib_fat` neumí** (dlouhé jméno, diakritika, hluboké vnoření) – soubor se buď uloží, nebo nástroj skončí kódem 1 s názvem souboru; nikdy ho tiše nevynechá. Test v Task 4.
5. **Hra volá `ConfigSave()`** (nastavení hlasitosti) – žádný pád na adrese flash; hodnota zůstane v paměti do konce běhu. Test v Task 1.

---

## Struktura souborů (nové a měněné)

```
cmake/picopad.cmake        # bez USE_CONFIG=0, + lib_config.c (přejmenování Config*), + nové host zdroje
host/config.c              # ConfigLoad/ConfigSave/ConfigClear bez flash
host/sdk_stubs.c           # + FlashErase/FlashProgram (fatal), FlashSize, FlashIDText, ADC_Temp, UsbPort*
host/libc_compat.c         # random()/srandom() (deterministické)
host/host_prelude.h        # deklarace random/srandom
host/audio.c               # PWMSnd* jen při USE_PWMSND, zdroj zvuku z GPIO15
host/hw/host_gpio.h        # GPIO_Out*/Flip/OutMask na pinu 15 -> host_gpio_snd_level()
host/sd.c                  # SD nad plat_sd_*
include/plat_api.h         # + plat_sd_sectors/plat_sd_read/plat_sd_write
platform/sdimage.c/.h      # obraz SD nad souborem (čisté C)
platform/cli.c, main.cpp   # --sd
tools/sdimg/               # picopad-sdimg: main (čisté C) + fat_ops.c (SDK strana) + config.h
tools/coverage/            # CMakeLists.txt (všechny hry), run.cmake, smoke.txt
tests/games/BEEP/, tests/games/SDTEST/, tests/games/CFGSAVE/
tests/sd/                  # vstupní složka pro obraz v testech
tests/wav_tone.py          # frekvence a špička WAV
docs/coverage.md
```

---

### Task 1: TINYGAME, ANTS a MULTISET se přeloží

**Files:**
- Modify: `cmake/picopad.cmake` (odebrat `USE_CONFIG=0`; přidat `_lib/src/lib_config.c` do `PICOPAD_SDK_SOURCES`; pro tuto TU `COMPILE_DEFINITIONS "ConfigLoad=sdk_unused_ConfigLoad;ConfigSave=sdk_unused_ConfigSave;ConfigClear=sdk_unused_ConfigClear"`; nové host zdroje `config.c`, `libc_compat.c`)
- Create: `host/config.c`, `host/libc_compat.c`
- Modify: `host/host_prelude.h`, `host/sdk_stubs.c`, `host/audio.c`
- Create: `tests/games/CFGSAVE/` (Makefile, config.h, include.h, src/main.cpp jako `tests/games/KEYWAIT`)
- Modify: `CMakeLists.txt` (hry a testy níže)

**Interfaces:**
- Consumes: `picopad_add_game`, `tests/headless_test.cmake` (způsob spouštění headless testů z M2).
- Produces: hry s `USE_CONFIG 1` mají `Config` s výchozími hodnotami; `long random(void)`, `void srandom(unsigned int)` dostupné z C i C++ kódu her; `FlashSize = 2*1024*1024`, `FlashIDText = "E660000000000000"` (16 hex číslic), `ADC_Temp()` = `27.0f`; USB Mini-Port „nepřipojen“ (`UsbPortConnected()` False, `Send` data zahodí a vrátí jejich počet, `Recv` vrátí 0, `SendReady` True). Signatury USB a proměnné `UsbPort` podle `_sdk/usb_inc` (implementátor je převezme z hlaviček).

- [ ] **Step 1: Testy (RED)** – v `CMakeLists.txt` přidat `picopad_add_game` pro `T_TINYBERT` (`PicoPad/TINYGAME/TINYBERT`), `T_INVADERS` (`PicoPad/TINYGAME/INVADERS`), `G_ANTS` (`PicoPad/GAME/ANTS`), `G_MULTISET` (`PicoPad/GAME/MULTISET`) a `CFGSAVE`. Ke každé repo hře headless test `repo_smoke_<jméno>` (label `repo`): skript `tests/headless/smoke.txt` = `3000 shot smoke` + `4500 quit`, očekává kód 0, řádek `end reason=quit` a snímek, který není jednobarevný (kontrola přes `tests/png_decode.py`, přidat mu volbu, která vypíše počet různých barev). Hra CFGSAVE: `ConfigSetVolume(50); ConfigSave(); ConfigLoad();` pak vypíše `Config.volume` jako text na displej, počká 1 s, `ResetToBootLoader()`; test `cfgsave` očekává kód 0 (žádný pád). Build zatím selže (A–E).
- [ ] **Step 2: Implementace** – podle Interfaces a Global Constraints. `host/audio.c`: odkazy na `PWMSndSlice/PWMSndChan` jen při `USE_PWMSND`. `random()`: vlastní deterministický generátor (stav začíná pevnou hodnotou, `srandom` ho nastaví), nesahá na `rand()` z CRT.
- [ ] **Step 3: Ověřit** – `cmake --build build` bez chyb; `ctest --test-dir build --output-on-failure` celé zelené včetně `tetris_determinism` (reference beze změny) a nových `repo_smoke_*`, `cfgsave`.
- [ ] **Step 4: Commit** – `feat(host): build TINYGAME, ANTS and MULTISET (config, random, USB port and flash stubs)`

### Task 2: AMANIA a vizuální kontrola TINYGAME

**Files:** podle nálezu (host/ nebo nic); `docs/coverage.md` jen pokud jde o vlastnost hry.

- [ ] **Step 1:** Přidat `T_AMANIA` do `CMakeLists.txt` stejně jako hry v Task 1 (trvale, s testem `repo_smoke_T_AMANIA`, pokud se ukáže chyba emulátoru) a zjistit, proč ve 3 s ukazuje jen bílý obdélník: snímky v 1, 3, 6, 10 s, porovnat s chováním hry ve zdrojácích (systematic debugging). Výsledek zapsat do reportu: chyba emulátoru (opravit s testem `repo_smoke_T_AMANIA` se snímkem, který není jednobarevný) / vlastnost hry (zdůvodnit odkazem na zdroják, test se nepřidává).
- [ ] **Step 2: Commit** (jen pokud se něco měnilo) – `fix(host): …` podle nálezu.

### Task 3: Zvuk z GPIO15 (bit-bang TINYGAME her)

**Files:**
- Modify: `host/hw/host_gpio.h` (`GPIO_Out0/Out1/Flip/Out/OutMask/OutAll` na pinu `PWMSND_GPIO` volají `host_gpio_snd_level(int level)`; ostatní piny dál no-op), `host/audio.c`, `host/host_internal.h`
- Modify: `tests/test_audio.c`
- Create: `tests/games/BEEP/`, `tests/wav_tone.py`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: mixer řízený emulovaným časem z M2 (`floor(t_us*rate/1e6)` vzorků, CAPTURE/RING), `host_now_us()`.
- Produces: `void host_gpio_snd_level(int level)` (host_internal.h) – zaznamená změnu úrovně pinu v čase `host_now_us()`; před změnou mixer dožene vzorky do tohoto času se starou úrovní. Vzorky se z GPIO berou, jen dokud není povolený PWM zvuk (viz Global Constraints); po `PWM_Enable` platí cesta z M2.

- [ ] **Step 1: Unit testy (RED)** v `tests/test_audio.c` (rate 44100, CAPTURE, virtuální čas):
  - `gpio_square_1khz`: 200× (úroveň 1, `WaitUs(500)`, úroveň 0, `WaitUs(500)`) → zachyceno 8820 vzorků (±1), hodnoty jen 0 a 16384 kromě vzorků na hranách (mezi), 200 náběžných hran ±1.
  - `gpio_idle_silent`: bez změn pinu 100 ms → všechny vzorky 0.
  - `gpio_ignored_after_pwm`: po `PWM_Enable` na slice zvuku změny pinu výstup nemění.
  - `waitus1_realtime`: reálný čas, 100 000× `WaitUs(1)` trvá < 300 ms reálného času (Review Focus 3).
- [ ] **Step 2: Hra BEEP** (vlastní Makefile, `USE_PWMSND 0` jako TINYGAME): 1 kHz obdélník na `PWMSND_GPIO` přes `gpio_put` + `sleep_us(500)` po dobu 500 ms, pak `ResetToBootLoader()`. Test `beep_wav` (headless, `--wav`): `tests/wav_tone.py` změří dominantní frekvenci přes průchody nulou po odečtení průměru → 1000 Hz ±2 %, špička ≥ 8000.
- [ ] **Step 3: Implementace** podle Interfaces.
- [ ] **Step 4: Ověřit** – celý ctest zelený (včetně determinismu a referencí). Ruční poslech není součástí úkolu (uživatel na konci).
- [ ] **Step 5: Commit** – `feat(audio): sample GPIO15 bit-bang sound for games without PWM sound`

### Task 4: Nástroj `picopad-sdimg`

**Files:**
- Create: `tools/sdimg/main.c` (čisté C: argumenty, soubory, rekurzivní procházení složky přes `FindFirstFileW`… nebo `_wfindfirst`), `tools/sdimg/fat_ops.c` (SDK strana: volá `lib_fat`), `tools/sdimg/fat_ops.h` (rozhraní v čistém C s vestavěnými typy), `tools/sdimg/config.h`, `tools/sdimg/include.h`
- Modify: `CMakeLists.txt` (cíl `picopad-sdimg`: `fat_ops.c` + `lib_fat.c` a nejmenší nutná sada SDK/host zdrojů s `picopad_sdk_compile_options`; **ne** `host/sd.c` – `SDReadSect/SDWriteSect/SDMediaSize/SDConnect/SDDisconnect/SDType` dodá nástroj sám nad souborem; bez SDL)
- Create: `tests/sd/TEST/IN.TXT` (obsah `HELLO SD`), `tests/sd/README.TXT`, `tests/sdimg_test.cmake`

**Interfaces:**
- Produces (CLI, kódy 0 OK / 1 chyba operace / 2 špatné argumenty):
  - `picopad-sdimg create IMG [--size MB] [--from DIR]` – vytvoří obraz (výchozí 64 MB), naformátuje přes `DiskFormat` z `lib_fat` a zkopíruje strom `DIR` do kořene.
  - `picopad-sdimg ls IMG [PATH]` – vypíše položky (`D JMÉNO` / `F JMÉNO VELIKOST`, jedna na řádek).
  - `picopad-sdimg get IMG PATH OUT` – zkopíruje soubor z obrazu.
  - `picopad-sdimg put IMG PATH IN` – zapíše soubor do obrazu (vytvoří, nebo přepíše).
- [ ] **Step 1: Test (RED)** `sdimg` (`tests/sdimg_test.cmake`): `create` z `tests/sd` → `ls /` obsahuje `D TEST` a `F README.TXT`; `get /TEST/IN.TXT` = původní bajty; `put` + `get` vrátí stejná data (4 KiB náhodných bajtů vygenerovaných v testu); `create --from` složky s dlouhým jménem souboru (`dlouhe-jmeno-souboru.txt`) → buď je soubor v `ls`, nebo kód 1 a jméno souboru ve výstupu (Review Focus 4; test přijme obojí, ale ne kód 0 bez souboru); neplatné argumenty → kód 2.
- [ ] **Step 2: Implementace.** Zda `lib_fat` umí dlouhá jména, ověřit ve zdroji a chování zapsat do README.
- [ ] **Step 3: Ověřit** – ctest zelený.
- [ ] **Step 4: Commit** – `feat(tools): picopad-sdimg creates and reads FAT SD images via lib_fat`

### Task 5: `--sd` v emulátoru

**Files:**
- Modify: `include/plat_api.h`, `host/sd.c`, `platform/cli.c/.h`, `platform/main.cpp`, `platform/headless.c` (zavření obrazu při všech koncích), `tests/plat_fake.c/.h`, `tests/test_cli.c`
- Create: `platform/sdimage.c/.h` (čisté C nad `FILE*` s `_fseeki64`/`_ftelli64`), `tests/games/SDTEST/`, `tests/sd_test.cmake`
- Modify: `cmake/picopad.cmake` (`sdimage.c` do exe her), `README.md`

**Interfaces:**
- Consumes: `picopad-sdimg` (Task 4) v testech.
- Produces:
  - `plat_api.h`: `unsigned int plat_sd_sectors(void);` (0 = karta není), `int plat_sd_read(unsigned int sector, unsigned char* buf512);`, `int plat_sd_write(unsigned int sector, const unsigned char* buf512);` (1 = OK, 0 = chyba; zápis mimo rozsah nebo do souboru jen pro čtení = 0).
  - `host/sd.c`: `SDConnect()` = `plat_sd_sectors() > 0`; `SDMediaSize()` = počet sektorů; `SDType` = SDHC; `SDReadSect/SDWriteSect` přes `plat_sd_*`; `SDDisconnect` jen zneplatní připojení.
  - CLI: `--sd PATH`; chybný obraz = kód 2 (Global Constraints).
- [ ] **Step 1: Testy (RED)**
  - `test_cli`: `--sd x.img` se parsuje; chybějící hodnota = chyba.
  - Hra SDTEST: `DiskAutoMount()`; bez karty nakreslí `NO CARD` a skončí; s kartou přečte `/TEST/IN.TXT`, zapíše `/TEST/OUT.TXT` s obsahem `OK:` + přečtený text, nakreslí ho a skončí `ResetToBootLoader()`.
  - `sd_test.cmake`: (a) `picopad-sdimg create` z `tests/sd` do build adresáře, SDTEST `--headless --sd img` → kód 0, `get /TEST/OUT.TXT` = `OK:HELLO SD`; (b) stejné, ale skript končí `quit` dřív, než hra skončí sama, po zápisu (hra po zápisu čeká 2 s) → soubor v obrazu je (Review Focus 2); (c) `--max-time` varianta totéž; (d) bez `--sd` → kód 0, obraz nevznikl; (e) neexistující soubor, soubor 1000 B, soubor 32 KiB, adresář → kód 2 a hláška; (f) obraz s atributem jen pro čtení → kód 0, `OUT.TXT` v obrazu není (Review Focus 1).
- [ ] **Step 2: Implementace** podle Interfaces; README sekce „SD karta“ (obraz, `--sd`, `picopad-sdimg`, zápisy jdou do souboru).
- [ ] **Step 3: Ověřit** – ctest zelený; ručně (bez okna) `G_PICTOR` z Task 6 zatím nespouštět.
- [ ] **Step 4: Commit** – `feat: SD card image support (--sd) backed by plat_sd_* sector I/O`

### Task 6: Pokrytí repa – runner a přehled

**Files:**
- Create: `tools/coverage/CMakeLists.txt` (cizí projekt přes `PicopadEmu.cmake` jako `tests/external`; projde `PicoPad/GAME/*` a `PicoPad/TINYGAME/*` s `Makefile` a přidá `G_<DIR>` / `T_<DIR>`), `tools/coverage/smoke.txt` (`3000 shot <jméno>`, `4500 quit`), `tools/coverage/run.cmake`
- Create: `docs/coverage.md`
- Modify: `CMakeLists.txt` (test `repo_coverage`, label `repo;slow`, TIMEOUT 1800, registrovaný jen s `-DPICOPAD_COVERAGE=ON`, protože staví všech 41 her), `README.md`

**Interfaces:**
- Produces: `cmake -DPICOLIBSDK_DIR=… -DOUT=<dir> -P tools/coverage/run.cmake` – nakonfiguruje a sestaví `tools/coverage` do `<OUT>/build` s `ninja -k 0` (selhání jedné hry nezastaví ostatní), každé postavené exe spustí headless se `smoke.txt` (`--shot-dir <OUT>/shots`, vnější limit 30 s), a zapíše `<OUT>/summary.md`: tabulka `hra | build | kód | end řádek | barev na snímku` a na konci `built=N ran=N nonuniform=N total=N`. Kód skriptu 0, když všechny hry projdou (build, kód 0, `reason=quit`, snímek s ≥ 2 barvami), jinak 1. Test `repo_coverage` ho spustí s `OUT=build/coverage`.
- [ ] **Step 1:** Napsat runner; spustit; všechny hry, které neprojdou, vyšetřit (systematic debugging) a opravit v host vrstvě s regresním testem, nebo zdůvodnit jako vlastnost hry.
- [ ] **Step 2:** `docs/coverage.md` ručně z `summary.md` + prohlédnutých snímků: tabulka všech 41 her (stav, co je na snímku, zvuk PWM / GPIO / žádný, SD, poznámka), seznam známých odchylek a datum/commit. Snímky se do repa necommitují.
- [ ] **Step 3: Ověřit** – build s `-DPICOPAD_COVERAGE=ON` a `ctest --test-dir build -L repo --output-on-failure` zelený; celý ctest zelený.
- [ ] **Step 4: Commit** – `feat(tools): repo coverage runner and docs/coverage.md`

---

## Ruční ověření uživatelem (po Task 6)

V okně: 2–3 TINYGAME hry (zvuk z GPIO, ovládání), ANTS, MULTISET (stránka se systémovými informacemi), PICTOR s `--sd` obrazem vytvořeným `picopad-sdimg` (zapíše se `/GAME/PICTOR.TOP`, ověřit `picopad-sdimg ls`). Výsledek se zapíše do README „Ověřeno“.
