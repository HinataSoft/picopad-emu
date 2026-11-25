cmake_minimum_required(VERSION 3.21)
project(picopad_emu C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICOPAD_TOOLCHAIN_DIR "${CMAKE_SOURCE_DIR}/../toolchain" CACHE PATH "Adresar s prenosnymi nastroji (SDL2, MinGW, Ninja)")
set(PICOLIBSDK_DIR "${CMAKE_SOURCE_DIR}/../PicoLibSDK" CACHE PATH "Adresar PicoLibSDK")

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

enable_testing()

# Prenositelne knihovny PicoLibSDK prelozene pro host; definuje i
# picopad_exe_runtime() (staticke runtime MinGW, kopie SDL2.dll vedle exe)
include(cmake/picopad.cmake)

add_executable(test_toolchain tests/test_toolchain.c)
target_include_directories(test_toolchain PRIVATE tests)
# SDL2main dodava WinMain, takze main() z testu funguje i na MinGW
target_link_libraries(test_toolchain PRIVATE SDL2::SDL2main SDL2::SDL2)
picopad_exe_runtime(test_toolchain SDL2)
add_test(NAME toolchain COMMAND test_toolchain)
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
    picopad_exe_runtime(test_${name})
    add_test(NAME ${name} COMMAND test_${name})
endfunction()
picopad_host_test(clock)
picopad_host_test(rom)
picopad_host_test(display)
picopad_host_test(keys)
picopad_host_test(audio)
picopad_host_test(exit)
picopad_host_test(tick)
# audio test uses a helper thread (windows.h kept out of the SDK-including TU)
target_sources(test_audio PRIVATE tests/thread_helper.c)
set_tests_properties(audio PROPERTIES TIMEOUT 30)
# tick test uses a helper thread for host_request_exit
target_sources(test_tick PRIVATE tests/thread_helper.c)
set_tests_properties(tick PROPERTIES TIMEOUT 30)
# stall test calls host_watchdog from a helper thread
picopad_host_test(stall)
target_sources(test_stall PRIVATE tests/thread_helper.c)
set_tests_properties(stall PROPERTIES TIMEOUT 30)

# Mapovani klaves a tap latch: cista C knihovna bez SDL a SDK
add_library(platform_keymap STATIC platform/keymap.c)
add_executable(test_keymap tests/test_keymap.c)
target_include_directories(test_keymap PRIVATE tests)
target_link_libraries(test_keymap PRIVATE platform_keymap)
picopad_exe_runtime(test_keymap)
add_test(NAME keymap COMMAND test_keymap)

# Hra TETRIS (zdrojaky beze zmen) -> TETRIS.exe
picopad_add_game(TETRIS ${PICOLIBSDK_DIR}/PicoPad/GAME/TETRIS)

# Prikazova radka herniho .exe (ciste C)
add_executable(test_cli tests/test_cli.c platform/cli.c)
target_include_directories(test_cli PRIVATE tests)
picopad_exe_runtime(test_cli)
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

# Exe bezi bez MinGW DLL (libwinpthread-1.dll apod.): test_clock s PATH jen
# na systemove adresare Windows musi skoncit kodem 0
add_test(NAME runtime_deps
         COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:test_clock>
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime_deps_test.cmake)
set_tests_properties(runtime_deps PROPERTIES TIMEOUT 60)

# Zapis PNG a WAV: cisty C bez SDL a SDK
add_library(platform_png STATIC platform/png.c)
add_executable(test_png tests/test_png.c)
target_include_directories(test_png PRIVATE tests)
target_link_libraries(test_png PRIVATE platform_png)
picopad_exe_runtime(test_png)
add_test(NAME png COMMAND test_png)
set_tests_properties(png PROPERTIES FIXTURES_SETUP png_files)

add_library(platform_wav STATIC platform/wav.c)
add_executable(test_wav tests/test_wav.c)
target_include_directories(test_wav PRIVATE tests)
target_link_libraries(test_wav PRIVATE platform_wav)
picopad_exe_runtime(test_wav)
add_test(NAME wav COMMAND test_wav)

# Dekodovani PNG pres zlib (test png zapise soubory do build adresare)
if(PICOPAD_PYTHON)
    add_test(NAME png_decode
             COMMAND ${CMAKE_COMMAND} -DPYTHON=${PICOPAD_PYTHON}
                     -DDECODE=${CMAKE_CURRENT_SOURCE_DIR}/tests/png_decode.py
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/png_decode_test.cmake
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(png_decode PROPERTIES FIXTURES_REQUIRED png_files)
endif()
