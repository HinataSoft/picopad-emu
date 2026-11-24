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

Cesty lze změnit cache proměnnými `PICOPAD_TOOLCHAIN_DIR` (výchozí `../toolchain`) a `PICOLIBSDK_DIR` (výchozí `../PicoLibSDK`).

## Build

```
cmake --preset default
cmake --build build
ctest --test-dir build --output-on-failure
```

Preset `default` používá generátor Ninja a překladače z `../toolchain`. Výsledné exe jsou slinkované s `-static` (runtime MinGW včetně libwinpthread je uvnitř exe, SDL2 zůstává jako DLL) a po sestavení se vedle nich kopíruje `SDL2.dll`, takže běží bez MinGW v PATH. Hlídá to test `runtime_deps` (spouští `test_clock` s PATH jen na systémové adresáře Windows).

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
- `--help` – nápověda

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

## Známá omezení

- Bez zvukového zařízení (SDL nenajde výstup, `audio_rate` 0) mixer neběží, takže `PlayingSound()` nikdy neskončí.
- Herní smyčka, která čeká jen na `PlayingSound()` nebo jen čte paměť bez volání SDK (bez `Time()`, `WaitMs()`, `KeyGet()` apod.), se zasekne – emulátor nemá přerušení a SysTick, klávesy i zvuk obsluhuje jen při volání SDK. Týká se např. her PACMAN, SOKOBAN a VEGASLOT z MULTISET. Obojí je plánované na milník 2.
- Nevkládat `<stdio.h>` spolu s `includes.h` SDK: `getchar` z `lib_print.h` koliduje s deklarací v `stdio.h`.

## Ověřeno

2026-11-24 ručně: `TETRIS.exe` na PC hraje s obrazem, zvukem a klávesami; `HELLO.uf2` nahraný přes BOOTSEL na Picopad 1.0 vykreslí RGB čtverečky, text „HELLO PICOPAD“ a zelený obdélník.
