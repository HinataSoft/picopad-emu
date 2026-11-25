# CMake toolchain file: i686 MinGW (WinLibs) + Ninja from the portable
# toolchain directory. Usage:
#   cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=<picopad-emu>/cmake/toolchain-mingw32.cmake
# PICOPAD_TOOLCHAIN_DIR (cache variable, -D) defaults to <picopad-emu>/../toolchain.
if(NOT PICOPAD_TOOLCHAIN_DIR)
    get_filename_component(PICOPAD_TOOLCHAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../toolchain" ABSOLUTE)
endif()

# try_compile projects re-include this file without the cache: pass the override on
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES PICOPAD_TOOLCHAIN_DIR)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER   "${PICOPAD_TOOLCHAIN_DIR}/mingw32/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "${PICOPAD_TOOLCHAIN_DIR}/mingw32/bin/g++.exe")
set(CMAKE_ASM_COMPILER "${PICOPAD_TOOLCHAIN_DIR}/mingw32/bin/gcc.exe")
set(CMAKE_MAKE_PROGRAM "${PICOPAD_TOOLCHAIN_DIR}/ninja/ninja.exe" CACHE FILEPATH "Ninja")
