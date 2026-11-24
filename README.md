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
