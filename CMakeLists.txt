cmake_minimum_required(VERSION 3.20)
project(picopad_emu C CXX)

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
picopad_sdk_objects(sdk_objs_test tests/config)
# C++ kontrola hlavicek SDK (vsechny hry jsou .cpp)
target_sources(sdk_objs_test PRIVATE tests/config/cpp_check.cpp)

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
