# ----------------------------------------------------------------------------
# Entry point for projects outside the emulator repo (e.g. picopad-games).
# Include it after project(<name> C CXX ASM):
#
#   include(${PICOPAD_EMU_DIR}/cmake/PicopadEmu.cmake)
#   picopad_add_game(HELLO games/HELLO)
#
# Sets defaults of the cache variables relative to the EMULATOR repo (not to
# the including project): PICOPAD_TOOLCHAIN_DIR = <emu>/../toolchain,
# PICOLIBSDK_DIR = <emu>/../PicoLibSDK. Finds SDL2 and includes picopad.cmake
# (picopad_add_game, picopad_sdk_objects, ...). Including it twice is harmless.
# ----------------------------------------------------------------------------
include_guard(GLOBAL)

get_filename_component(PICOPAD_EMU_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(PICOPAD_TOOLCHAIN_DIR "${PICOPAD_EMU_ROOT}/../toolchain" CACHE PATH "Adresar s prenosnymi nastroji (SDL2, MinGW, Ninja)")
set(PICOLIBSDK_DIR "${PICOPAD_EMU_ROOT}/../PicoLibSDK" CACHE PATH "Adresar PicoLibSDK")

# SDL2 devel (mingw, i686); sdl2-config.cmake definuje SDL2::SDL2 a SDL2::SDL2main
file(GLOB _sdl2_roots "${PICOPAD_TOOLCHAIN_DIR}/SDL2-*/i686-w64-mingw32")
if(NOT _sdl2_roots)
    message(FATAL_ERROR "SDL2 nenalezeno: chybi ${PICOPAD_TOOLCHAIN_DIR}/SDL2-<verze>/i686-w64-mingw32. "
        "Stahnete SDL2-devel-2.32.10-mingw.zip (github.com/libsdl-org/SDL/releases) a rozbalte do "
        "${PICOPAD_TOOLCHAIN_DIR} (viz README.md, sekce Toolchain), pripadne nastavte PICOPAD_TOOLCHAIN_DIR.")
endif()
list(SORT _sdl2_roots)
list(REVERSE _sdl2_roots)
list(GET _sdl2_roots 0 _sdl2_root)
set(SDL2_DIR "${_sdl2_root}/lib/cmake/SDL2" CACHE PATH "SDL2 cmake config" FORCE)
# exe jsou konzolove aplikace (bez -mwindows): hlasky na stderr, Ctrl+C a
# navratovy kod funguji i z cmd
set(SDL2_NO_MWINDOWS ON)
find_package(SDL2 REQUIRED CONFIG)

# Prenositelne knihovny PicoLibSDK prelozene pro host; definuje i
# picopad_exe_runtime() (staticke runtime MinGW, kopie SDL2.dll vedle exe)
include("${CMAKE_CURRENT_LIST_DIR}/picopad.cmake")
