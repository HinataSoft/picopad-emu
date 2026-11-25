# picopad-emu

PC emulator handheldu Picopad (RP2040). Cílem je 32bitový (i686) build pod Windows pomocí MinGW a SDL2.

## Toolchain

Nástroje jsou přenosné a leží v sourozeneckém adresáři `../toolchain/` (nic se neinstaluje do systému ani do PATH). Na čistém počítači stačí stáhnout a rozbalit tyto archivy (ověřené verze):

| Co | Odkud | Archiv | Rozbalit do |
|---|---|---|---|
| GCC 16.2.0 pro i686 (WinLibs, posix threads, dwarf, UCRT, r3) | https://github.com/brechtsanders/winlibs_mingw/releases (release GCC 16.2.0 + MinGW-w64 14.0.0 UCRT, r3; ze seznamu příloh vybrat přesně tento zip) | `winlibs-i686-posix-dwarf-gcc-16.2.0-mingw-w64ucrt-14.0.0-r3.zip` | `../toolchain/mingw32/` (archiv obsahuje adresář `mingw32`, takže `../toolchain/mingw32/bin/gcc.exe`) |
| Ninja 1.13.2 | https://github.com/ninja-build/ninja/releases/tag/v1.13.2 | `ninja-win.zip` | `../toolchain/ninja/` (`../toolchain/ninja/ninja.exe`) |
| SDL2 2.32.10 devel pro MinGW | https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10 | `SDL2-devel-2.32.10-mingw.zip` | `../toolchain/` (vznikne `../toolchain/SDL2-2.32.10/`, používá se podadresář `i686-w64-mingw32/`) |
| CMake 3.21 nebo novější | https://cmake.org/download/ | instalátor nebo zip | kamkoli, volá se plnou cestou, např. `C:\Progs\CMake\bin\cmake.exe` |

Výsledná struktura:

- `../toolchain/mingw32/` – WinLibs GCC (cíl i686-w64-mingw32)
- `../toolchain/ninja/ninja.exe` – Ninja
- `../toolchain/SDL2-<verze>/i686-w64-mingw32/` – SDL2 devel (včetně `bin/SDL2.dll`); při více verzích se vezme nejvyšší, když žádná není, configure skončí srozumitelnou chybou
- PicoLibSDK v `../PicoLibSDK/` (beze změn)

Pro build pro zařízení (`tools\build_device.ps1`, test `hello_device`) je navíc potřeba ARM GNU Toolchain (`arm-none-eabi-gcc`, ověřeno 14.2.Rel1) a Python 3.

Cesty lze změnit cache proměnnými `PICOPAD_TOOLCHAIN_DIR` (výchozí `../toolchain`) a `PICOLIBSDK_DIR` (výchozí `../PicoLibSDK`); výchozí hodnoty se počítají od repa emulátoru, ne od projektu, který ho používá.

## Build

```
cmake --preset default
cmake --build build
ctest --test-dir build --output-on-failure
```

Preset `default` používá generátor Ninja a toolchain soubor `cmake/toolchain-mingw32.cmake` (překladače a Ninja z `../toolchain`). Výsledné exe jsou slinkované s `-static` (runtime MinGW včetně libwinpthread je uvnitř exe, SDL2 zůstává jako DLL) a po sestavení se vedle nich kopíruje `SDL2.dll`, takže běží bez MinGW v PATH. Hlídá to test `runtime_deps` (spouští `test_clock` s PATH jen na systémové adresáře Windows).

## Spuštění hry (TETRIS)

Build vytvoří `build\TETRIS.exe` (hra `PicoPad/GAME/TETRIS` z PicoLibSDK přeložená beze změn) a vedle něj `SDL2.dll`. Spuštění:

```
build\TETRIS.exe [volby]
```

Okno má titulek podle jména hry a velikost 320×240 × měřítko (výchozí 3 = 960×720, škálování nearest-neighbor). Exe je konzolová aplikace: chyby a hlášky hry (`panic`) jdou na stderr.

Volby (lze psát i jako `--volba=hodnota`):

- `--scale N` – měřítko okna 1..8 (výchozí 3)
- `--keys letters|layout` – mapování kláves (výchozí `letters`)
- `--fast-display` – vypne model přenosu na displej (SPI 24 MHz)
- `--seed N` – semínko náhodných čísel (výchozí z času)
- `--max-time T` – po čase T skončí s kódem 3; T = `2000`, `2000ms` nebo `2s`
- `--trace-keys` – při každé změně stavu kláves vypíše na stderr `keys t=<ms> bits=0x<hex>`
- `--headless`, `--script`, `--shot-dir`, `--wav` – běh bez okna, viz [Headless režim](#headless-režim)
- `--help` – nápověda

Při startu vypíše na stderr `seed=<n>`; stejný běh jde zopakovat s `--seed <n>`.

Klávesy:

| Picopad | `--keys letters` (výchozí) | `--keys layout` |
|---|---|---|
| D-pad | šipky | šipky |
| A | A | Z |
| B | B | X |
| X | X | A |
| Y | Y, Esc | S, Esc |

Mapování `letters` se řídí popiskem klávesy podle aktivního rozložení klávesnice (na české QWERTZ funguje klávesa s popiskem Y). V rozložení `layout` se klávesy berou podle fyzické polohy (pozice A S / Z X na americké klávesnici, na QWERTZ tedy A S / Y X) a tvoří čtverec, který kopíruje fyzické rozmístění tlačítek X Y / A B na Picopadu.

Návratové kódy: 0 = hra skončila přes `ResetToBootLoader` (v TETRISu podržené Y), nebo zavření okna / Ctrl+C; 1 = fatální chyba (`panic`); 2 = chybné argumenty; 3 = vypršel `--max-time`.

## Headless režim

S `--headless` běží každé herní exe bez okna a bez zvukového zařízení ve virtuálním čase: emulovaný čas neběží podle hodin PC, ale podle toho, kolik ho hra „spotřebuje“ (`WaitMs`, přenos na displej, čekání na `Time()`), takže běh je mnohonásobně rychlejší než reálný a se stejným skriptem a semínkem bajtově opakovatelný. Slouží pro automatické testy her.

```
build\HELLO.exe --headless --script tests\headless\hello.txt --shot-dir shots
```

Volby:

- `--headless` – běh bez okna; výchozí semínko 0 a `--max-time 60s` (emulovaného času)
- `--script FILE` – vstupní skript (klávesy, snímky, konec)
- `--shot-dir DIR` – adresář pro snímky `<DIR>/<jméno>.png` (výchozí `.`), vytvoří se včetně rodičů při prvním snímku
- `--wav FILE` – zapíše zvuk hry do WAV (PCM, mono, 16 bit, 44100 Hz); jen s `--headless`
- `--trace-keys` – do logu přidá řádek `keys` při každé změně kláves
- `--seed N`, `--max-time T`, `--fast-display` – jako v interaktivním režimu (`--fast-display` vypne model přenosu na displej, jinak trvá celá obrazovka 51,2 ms)

Skript je textový soubor, jeden příkaz na řádek, `#` začíná komentář. Čas je v ms emulovaného času od startu, události se provádějí na SysTicku (po 5 ms) v pořadí času a pak pořadí v souboru:

```
# čas  příkaz
200  shot title        # snímek displeje do title.png (jméno: písmena, číslice, _ a -)
300  press A           # stisk klávesy (UP DOWN LEFT RIGHT A B X Y)
400  release A         # uvolnění
500  hold B 100        # stisk na 100 ms (= press v 500, release v 600)
1000 quit              # konec, kód 0
```

Uvolnění klávesy stisknuté ve stejném ticku se odloží o jeden tick (aby hra stisk vůbec viděla), a s ním i další události téhož ticku.

Log jde na stdout, jeden řádek = jedna událost (`t` = emulovaný čas v ms); hlášky hry a `stall` jdou na stderr:

```
start game=HELLO seed=0 rate=44100
shot t=200 file=shots/title.png
keys t=300 bits=0x40
end reason=quit t=1000 code=0 stalls=0 pending=0
```

- `keys` jen s `--trace-keys`; `bits` = bit i odpovídá klávese i+1 (UP=0x01, LEFT=0x02, RIGHT=0x04, DOWN=0x08, X=0x10, Y=0x20, A=0x40, B=0x80)
- `end reason=`: `quit` (příkaz `quit`), `exit` (hra skončila sama přes `ResetToBootLoader` nebo návratem z `main`), `fatal` (`panic`, chyba zápisu PNG/WAV, `--shot-dir` není adresář), `max-time` (vypršel `--max-time`)
- `stalls` = počet zásahů hlídače (viz níže), `pending` = počet nezpracovaných událostí skriptu (např. hra skončila dřív)

Návratové kódy: 0 = `quit` nebo hra skončila sama; 1 = fatální chyba; 2 = chybné argumenty nebo chyba ve skriptu (hláška s číslem řádku na stderr); 3 = vypršel `--max-time`.

Omezení determinismu: smyčka hry bez volání SDK (např. `while (PlayingSound()) {}`) neposouvá emulovaný čas; takovou smyčku po 100 ms reálného času odblokuje hlídač (`host_watchdog`), který posouvá čas po jednom ticku z vlastního vlákna. Od prvního zásahu (`stall:` na stderr, `stalls=N` > 0 v řádku `end`) je běh jen „best effort“ – kolik se toho ve smyčce stihne, závisí na rychlosti PC. Běh s `stalls=0` je plně deterministický. Když konec (`quit`, `max-time`) nastane během zásahu hlídače a hra smyčku do 1 s reálného času neopustí, emulátor vypíše řádek `end` a proces ukončí sám s týmž kódem.

## Hra mimo strom SDK (HELLO) a build pro zařízení

`examples/HELLO` je minimální hra mimo PicoLibSDK (vlastní `Makefile` s `PICO_ROOT_PATH = ../../../PicoLibSDK`). Z jedněch zdrojáků vzniká `build\HELLO.exe` pro emulátor i `HELLO.uf2` pro Picopad.

Build pro zařízení (potřebuje `arm-none-eabi-gcc`; když není v PATH, skript ho najde v `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\*\bin`):

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_device.ps1 -GameDir examples\HELLO -Device picopad10
```

Název výstupu je jméno adresáře hry, výsledek je `examples\HELLO\HELLO.uf2` (s CRC pro loader, přidaným `LoaderCrc.exe`). Do stromu SDK se nic nekopíruje. Volitelný `-SdkDir` mění cestu k SDK (výchozí `..\PicoLibSDK`). Kontrola UF2:

```
C:\Progs\anaconda3\python.exe -I tools\uf2check.py examples\HELLO\HELLO.uf2
```

Ověří UF2 bloky a rodinu čipu, značky PPAD/ppad v hlavičce aplikace a její CRC; vrací 0 při shodě, jinak 1. Test `hello_device` (label `device`) dělá obojí; bez ARM toolchainu se při configure neregistruje a jde vynechat i `ctest -LE device`. Výstupy buildu v `examples\HELLO` jsou v `.gitignore`.

## Použití z jiného repa

Hry generované mimo toto repo (např. sourozenecké repo `picopad-games`) se do emulátorových `.exe` překládají zahrnutím jediného souboru `cmake/PicopadEmu.cmake`. Minimální `CMakeLists.txt` cizího projektu:

```cmake
cmake_minimum_required(VERSION 3.21)
project(mygames C CXX ASM)
include(${PICOPAD_EMU_DIR}/cmake/PicopadEmu.cmake)
picopad_add_game(HELLO games/HELLO)
```

Relativní cesta hry (a `config_dir` u `picopad_sdk_objects`) se bere od adresáře volajícího `CMakeLists.txt`. Každá hra vznikne jako `<jméno>.exe` s `SDL2.dll` vedle sebe (stejně jako `TETRIS.exe`). Configure a build:

```
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=D:/Work/Prog/Picopad/picopad-emu/cmake/toolchain-mingw32.cmake -DPICOPAD_EMU_DIR=D:/Work/Prog/Picopad/picopad-emu
cmake --build build
```

- `PICOPAD_EMU_DIR` – kořen repa picopad-emu; předává se příkazem `-D` (a v `CMakeLists.txt` cizího projektu se používá jen k `include`).
- `PICOLIBSDK_DIR` – kořen PicoLibSDK, výchozí `<picopad-emu>/../PicoLibSDK`. SDK se nemění a nic se do něj nezapisuje.
- `PICOPAD_TOOLCHAIN_DIR` – adresář s `mingw32/`, `ninja/` a `SDL2-<verze>/`, výchozí `<picopad-emu>/../toolchain`. Čte ho `toolchain-mingw32.cmake` (překladače, Ninja) i `PicopadEmu.cmake` (SDL2).

Obě cesty (`PICOLIBSDK_DIR`, `PICOPAD_TOOLCHAIN_DIR`) jdou přepsat parametrem `-D` při configure. Opakované `include` `PicopadEmu.cmake` je neškodné. Funkčnost hlídá test `external_project` (label `slow`): `tests/external` je takový minimální cizí projekt, test ho nakonfiguruje do `build/external`, sestaví a spustí `HELLO.exe --headless --max-time 1s` s očekávaným kódem 3 (`ctest -LE slow` ho vynechá).

## Známá omezení

- Herní smyčka bez volání SDK (čeká jen na `PlayingSound()` nebo jen čte paměť) se nezasekne: emulátor nemá přerušení, takže ji po 100 ms odblokuje hlídač a dál obsluhuje SysTick, klávesy a zvuk z vlastního vlákna (po 20 ms; v headless režimu po jednom ticku, tj. 4× pomaleji než reálný čas) (týká se např. her PACMAN, SOKOBAN a VEGASLOT z MULTISET).
- Nevkládat `<stdio.h>` spolu s `includes.h` SDK: `getchar` z `lib_print.h` koliduje s deklarací v `stdio.h`.

## Ověřeno

2026-11-24 ručně: `TETRIS.exe` na PC hraje s obrazem, zvukem a klávesami; `HELLO.uf2` nahraný přes BOOTSEL na Picopad 1.0 vykreslí RGB čtverečky, text „HELLO PICOPAD“ a zelený obdélník.
