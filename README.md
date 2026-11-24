# picopad-emu

PC emulator handheldu Picopad (RP2040). Cílem je 32bitový (i686) build pod Windows pomocí MinGW a SDL2.

## Toolchain

Nástroje jsou přenosné a leží v sourozeneckém adresáři `../toolchain/` (nic se neinstaluje do systému ani do PATH):

- `../toolchain/mingw32/` – WinLibs GCC (cíl i686-w64-mingw32)
- `../toolchain/ninja/ninja.exe` – Ninja
- `../toolchain/SDL2-<verze>/i686-w64-mingw32/` – SDL2 devel (včetně `bin/SDL2.dll`)
- CMake (3.20+) se volá z místa instalace, např. `C:\Progs\CMake\bin\cmake.exe`

Cesty lze změnit cache proměnnými `PICOPAD_TOOLCHAIN_DIR` (výchozí `../toolchain`) a `PICOLIBSDK_DIR` (výchozí `../PicoLibSDK`).

## Build

```
cmake --preset default
cmake --build build
ctest --test-dir build --output-on-failure
```

Preset `default` používá generátor Ninja a překladače z `../toolchain`. Výsledné exe jsou slinkované s `-static-libgcc -static-libstdc++` a po sestavení se vedle nich kopíruje `SDL2.dll`, takže běží bez MinGW v PATH.

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

V rozložení `layout` tvoří klávesy A S / Z X čtverec, který kopíruje fyzické rozmístění tlačítek X Y / A B na Picopadu.

Návratové kódy: 0 = hra skončila přes `ResetToBootLoader` (v TETRISu podržené Y), nebo zavření okna / Ctrl+C; 1 = fatální chyba (`panic`); 2 = chybné argumenty; 3 = vypršel `--max-time`.

TODO: Ruční ověření uživatelem: čeká
