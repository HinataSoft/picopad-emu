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
# exe jsou konzolove aplikace (bez -mwindows): hlasky na stderr, Ctrl+C a
# navratovy kod funguji i z cmd
set(SDL2_NO_MWINDOWS ON)
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
picopad_host_test(audio)
# audio test uses a helper thread (windows.h kept out of the SDK-including TU)
target_sources(test_audio PRIVATE tests/thread_helper.c)
set_tests_properties(audio PROPERTIES TIMEOUT 30)

# Mapovani klaves a tap latch: cista C knihovna bez SDL a SDK
add_library(platform_keymap STATIC platform/keymap.c)
add_executable(test_keymap tests/test_keymap.c)
target_include_directories(test_keymap PRIVATE tests)
target_link_libraries(test_keymap PRIVATE platform_keymap)
add_test(NAME keymap COMMAND test_keymap)

# Hra TETRIS (zdrojaky beze zmen) -> TETRIS.exe
picopad_add_game(TETRIS ${PICOLIBSDK_DIR}/PicoPad/GAME/TETRIS)

# Prikazova radka herniho .exe (ciste C)
add_executable(test_cli tests/test_cli.c platform/cli.c)
target_include_directories(test_cli PRIVATE tests)
add_test(NAME cli COMMAND test_cli)

# Cteni radku SRC +=/CSRC += z Makefile hry
add_test(NAME parse_makefile
         COMMAND ${CMAKE_COMMAND} -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/parse_makefile_test.cmake)

# TETRIS bezi 2 s (okno + zvuk) a skonci kodem 3 (vyprsel --max-time)
add_test(NAME tetris_smoke
         COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:TETRIS> -DEXPECT=3
                 "-DARGS=--max-time;2s;--fast-display"
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
set_tests_properties(tetris_smoke PROPERTIES TIMEOUT 20)

# Ukazkova hra mimo strom SDK -> HELLO.exe (a HELLO.uf2 pres tools/build_device.ps1)
picopad_add_game(HELLO ${CMAKE_SOURCE_DIR}/examples/HELLO)
add_test(NAME hello_smoke
         COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:HELLO> -DEXPECT=3
                 "-DARGS=--max-time;1s;--fast-display"
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
set_tests_properties(hello_smoke PROPERTIES TIMEOUT 20)

# Build pro zarizeni (ARM GCC) + kontrola UF2, label "device" (ctest -LE device
# ho vynecha). Kdyz neni arm-none-eabi-gcc ani v PATH, ani v "Arm GNU Toolchain"
# v Program Files, test se vubec neregistruje (hlaska pri configure).
file(GLOB _picopad_arm
     "C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/*/bin/arm-none-eabi-gcc.exe")
find_program(_picopad_arm_in_path arm-none-eabi-gcc)
find_program(PICOPAD_PYTHON NAMES python python3 HINTS C:/Progs/anaconda3)
if((_picopad_arm OR _picopad_arm_in_path) AND PICOPAD_PYTHON)
    add_test(NAME hello_device
             COMMAND ${CMAKE_COMMAND}
                     -DGAME_DIR=${CMAKE_CURRENT_SOURCE_DIR}/examples/HELLO
                     -DSDK_DIR=${PICOLIBSDK_DIR}
                     -DPYTHON=${PICOPAD_PYTHON}
                     -DTOOLS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tools
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/hello_device_test.cmake)
    set_tests_properties(hello_device PROPERTIES LABELS device TIMEOUT 600)
else()
    message(STATUS "arm-none-eabi-gcc or python not found: hello_device test not registered")
endif()
