cmake_minimum_required(VERSION 3.20)
project(picopad_emu C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICOPAD_TOOLCHAIN_DIR "${CMAKE_SOURCE_DIR}/../toolchain" CACHE PATH "Adresar s prenosnymi nastroji (SDL2, MinGW, Ninja)")
set(PICOLIBSDK_DIR "${CMAKE_SOURCE_DIR}/../PicoLibSDK" CACHE PATH "Adresar PicoLibSDK")

# SDL2 devel (mingw, i686); sdl2-config.cmake definuje SDL2::SDL2 a SDL2::SDL2main
file(GLOB _sdl2_roots "${PICOPAD_TOOLCHAIN_DIR}/SDL2-*/i686-w64-mingw32")
list(SORT _sdl2_roots)
list(REVERSE _sdl2_roots)
list(GET _sdl2_roots 0 _sdl2_root)
set(SDL2_DIR "${_sdl2_root}/lib/cmake/SDL2" CACHE PATH "SDL2 cmake config" FORCE)
find_package(SDL2 REQUIRED CONFIG)

# Vysledne exe bezi bez MinGW bin v PATH
add_link_options(-static-libgcc -static-libstdc++)

enable_testing()

# Pomocna funkce: po sestaveni zkopiruje SDL2.dll vedle exe
function(picopad_copy_sdl2_dll target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:${target}>)
endfunction()

add_executable(test_toolchain tests/test_toolchain.c)
target_include_directories(test_toolchain PRIVATE tests)
# SDL2main dodava WinMain, takze main() z testu funguje i na MinGW
target_link_libraries(test_toolchain PRIVATE SDL2::SDL2main SDL2::SDL2)
picopad_copy_sdl2_dll(test_toolchain)
add_test(NAME toolchain COMMAND test_toolchain)

# Prenositelne knihovny PicoLibSDK prelozene pro host
include(cmake/picopad.cmake)
# SDK + host vrstva s testovaci konfiguraci (pouzivaji ji unit testy)
picopad_sdk_objects(sdk_objs_test tests/config)
# C++ kontrola hlavicek SDK (vsechny hry jsou .cpp); jen preklad
add_library(sdk_cpp_check OBJECT tests/config/cpp_check.cpp)
picopad_sdk_compile_options(sdk_cpp_check tests/config)

# Falesna platforma pro unit testy (jen plat_api.h, bez SDK)
add_library(plat_fake STATIC tests/plat_fake.c)

# Unit test host vrstvy: <name> = tests/test_<name>.c, hra je definovana v testu
function(picopad_host_test name)
    add_executable(test_${name} tests/test_${name}.c $<TARGET_OBJECTS:sdk_objs_test>)
    picopad_sdk_compile_options(test_${name} tests/config)
    target_include_directories(test_${name} PRIVATE tests)
    target_link_libraries(test_${name} PRIVATE plat_fake)
    add_test(NAME ${name} COMMAND test_${name})
endfunction()
picopad_host_test(clock)
picopad_host_test(rom)
picopad_host_test(display)
picopad_host_test(keys)

# Mapovani klaves a tap latch: cista C knihovna bez SDL a SDK
add_library(platform_keymap STATIC platform/keymap.c)
add_executable(test_keymap tests/test_keymap.c)
target_include_directories(test_keymap PRIVATE tests)
target_link_libraries(test_keymap PRIVATE platform_keymap)
add_test(NAME keymap COMMAND test_keymap)

# Realna kontrola: zdrojaky hry TETRIS (beze zmen) se prelozi s host hlavickami
set(_tetris "${PICOPAD_SDK_ABS}/PicoPad/GAME/TETRIS")
add_library(tetris_objs_check OBJECT
    ${_tetris}/src/main.cpp
    ${_tetris}/img/tiles.cpp
    ${_tetris}/snd/brick.cpp
    ${_tetris}/snd/gameover.cpp
    ${_tetris}/snd/levelup.cpp
    ${_tetris}/snd/line.cpp
)
picopad_sdk_compile_options(tetris_objs_check "${_tetris}")
target_compile_definitions(tetris_objs_check PRIVATE main=game_main)
