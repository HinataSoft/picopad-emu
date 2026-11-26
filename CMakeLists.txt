cmake_minimum_required(VERSION 3.21)
project(picopad_emu C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# PICOPAD_TOOLCHAIN_DIR, PICOLIBSDK_DIR, SDL2 a picopad.cmake (knihovny SDK,
# picopad_add_game, platform_png/wav/script) - stejny vstup jako pro cizi repa
include(cmake/PicopadEmu.cmake)

enable_testing()

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
add_executable(test_png tests/test_png.c)
target_include_directories(test_png PRIVATE tests)
target_link_libraries(test_png PRIVATE platform_png)
picopad_exe_runtime(test_png)
add_test(NAME png COMMAND test_png)
set_tests_properties(png PROPERTIES FIXTURES_SETUP png_files)

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

# Skript headless behu: parser a prehravac (cisty C)
add_executable(test_script tests/test_script.c)
target_include_directories(test_script PRIVATE tests)
target_link_libraries(test_script PRIVATE platform_script)
picopad_exe_runtime(test_script)
add_test(NAME script COMMAND test_script)

# Testovaci hry pro headless rezim (jen pro emulator)
picopad_add_game(KEYWAIT ${CMAKE_SOURCE_DIR}/tests/games/KEYWAIT)
picopad_add_game(STALL ${CMAKE_SOURCE_DIR}/tests/games/STALL)
picopad_add_game(HANG ${CMAKE_SOURCE_DIR}/tests/games/HANG)
picopad_add_game(WDPANIC ${CMAKE_SOURCE_DIR}/tests/games/WDPANIC)
picopad_add_game(CFGSAVE ${CMAKE_SOURCE_DIR}/tests/games/CFGSAVE)

# Hry z repa PicoLibSDK (zdrojaky beze zmen), ktere se musi prelozit a bezet
picopad_add_game(T_TINYBERT ${PICOLIBSDK_DIR}/PicoPad/TINYGAME/TINYBERT)
picopad_add_game(T_INVADERS ${PICOLIBSDK_DIR}/PicoPad/TINYGAME/INVADERS)
picopad_add_game(G_ANTS ${PICOLIBSDK_DIR}/PicoPad/GAME/ANTS)
picopad_add_game(G_MULTISET ${PICOLIBSDK_DIR}/PicoPad/GAME/MULTISET)

# picopad_headless_test(<name> <game target> <exit code> [ARGS ...] [STDOUT ...]
#     [STDERR ...] [EXIST ...] [ABSENT ...] [COMPARE f ref ...] [MAKE_FILE f] [WAV f]
#     [MAX_SECONDS n])
# runs tests/headless_test.cmake in its own directory build/headless/<name>
set(PICOPAD_HL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/headless")
function(picopad_headless_test name game expect)
    cmake_parse_arguments(HT "" "MAKE_FILE;WAV;MAX_SECONDS;COLORS" "ARGS;STDOUT;STDERR;EXIST;ABSENT;COMPARE" ${ARGN})
    set(_work "${CMAKE_CURRENT_BINARY_DIR}/headless/${name}")
    add_test(NAME ${name}
             COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${game}> -DEXPECT=${expect}
                     -DWORK_DIR=${_work}
                     "-DARGS=${HT_ARGS}"
                     "-DSTDOUT_MATCH=${HT_STDOUT}"
                     "-DSTDERR_MATCH=${HT_STDERR}"
                     "-DFILES_EXIST=${HT_EXIST}"
                     "-DFILES_ABSENT=${HT_ABSENT}"
                     "-DCOMPARE=${HT_COMPARE}"
                     "-DMAKE_FILE=${HT_MAKE_FILE}"
                     "-DWAV=${HT_WAV}"
                     "-DMAX_SECONDS=${HT_MAX_SECONDS}"
                     "-DCOLORS=${HT_COLORS}"
                     "-DPYTHON=${PICOPAD_PYTHON}"
                     "-DDECODE=${CMAKE_CURRENT_SOURCE_DIR}/tests/png_decode.py"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/headless_test.cmake)
    set_tests_properties(${name} PROPERTIES TIMEOUT 40)
endfunction()

# hra skonci sama kratce po stisku A: "shot after" a "quit" zustanou nezpracovane
picopad_headless_test(keywait_script KEYWAIT 0
    ARGS --headless --trace-keys --script ${PICOPAD_HL_DIR}/keywait.txt
    STDOUT "^start game=KEYWAIT seed=0 rate=44100$"
           "^shot t=200 file=\./before\.png$"
           "^keys t=300 bits=0x40$"
           "^keys t=400 bits=0x00$"
           "^end reason=exit t=[0-9]+ code=0 stalls=0 pending=2$"
    EXIST before.png
    ABSENT after.png)
picopad_headless_test(keywait_noscript KEYWAIT 3
    ARGS --headless --max-time 3s
    STDOUT "^end reason=max-time t=3000 code=3 stalls=0 pending=0$")
picopad_headless_test(stall_headless STALL 0
    ARGS --headless
    STDOUT "^end reason=exit t=[0-9]+ code=0 stalls=[1-9][0-9]* pending=0$")
# hra po vykresleni smycka bez volani SDK: cas posouva jen hlidac (~20 ms
# realneho casu na tick), quit v 300 ms ceka na zabiti po HANG_KILL_MS (1 s)
picopad_headless_test(hang_quit HANG 0
    ARGS --headless --script ${PICOPAD_HL_DIR}/hang.txt
    STDOUT "^end reason=quit t=300 code=0 stalls=[1-9][0-9]* pending=0$"
    MAX_SECONDS 8)
# panic v kroku hlidace (mixer na jinem vlakne): zadny pad, konec fatal/1,
# i kdyz hra uz nikdy nezavola SDK (konci zabitim po HANG_KILL_MS)
picopad_headless_test(watchdog_panic WDPANIC 1
    ARGS --headless
    STDOUT "^end reason=fatal t=[0-9]+ code=1 stalls=[1-9][0-9]* pending=0$"
    STDERR "WDPANIC boom"
    MAX_SECONDS 8)
picopad_headless_test(hello_headless HELLO 0
    ARGS --headless --script ${PICOPAD_HL_DIR}/hello.txt
    STDOUT "^start game=HELLO seed=0 rate=44100$"
           "^shot t=200 file=\./title\.png$"
           "^end reason=quit t=300 code=0 stalls=0 pending=0$"
    COMPARE title.png ${CMAKE_CURRENT_SOURCE_DIR}/tests/ref/hello_title.png)
picopad_headless_test(shot_dir_nested HELLO 0
    ARGS --headless --script ${PICOPAD_HL_DIR}/hello.txt
         --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/headless/shot_dir_nested/shots/a/b
    STDOUT "^shot t=200 file=.*/shots/a/b/title\.png$"
    EXIST shots/a/b/title.png)
picopad_headless_test(shot_dir_bad HELLO 1
    ARGS --headless --script ${PICOPAD_HL_DIR}/hello.txt --shot-dir notadir
    MAKE_FILE notadir
    STDOUT "^end reason=fatal t=200 code=1 "
    STDERR "notadir")
picopad_headless_test(script_error HELLO 2
    ARGS --headless --script ${PICOPAD_HL_DIR}/bad.txt
    STDERR "line 2")
picopad_headless_test(wav_headless KEYWAIT 0
    ARGS --headless --script ${PICOPAD_HL_DIR}/keywait_wav.txt --wav out.wav
    STDOUT "^end reason=exit t=[0-9]+ code=0 "
    WAV out.wav)

# Akceptacni test milniku 2: TETRIS ve skriptovanem headless behu je bajtove
# opakovatelny a shoduje se s referencnimi PNG (tests/ref/tetris_*.png);
# jiny seed zmeni aspon jeden snimek rozehrane hry
add_test(NAME tetris_determinism
         COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:TETRIS>
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/headless/tetris_determinism
                 -DSCRIPT=${PICOPAD_HL_DIR}/tetris.txt
                 -DREF_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/ref
                 -DPREFIX=tetris
                 "-DSHOTS=title;game1;game2"
                 "-DSEED_SHOTS=game1;game2"
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/determinism_test.cmake)
set_tests_properties(tetris_determinism PROPERTIES TIMEOUT 120)

# Cizi projekt (vzor picopad-games): tests/external zahrnuje PicopadEmu.cmake,
# sestavi HELLO.exe do build/external a spusti ho headless (preklada SDK znovu,
# proto label slow)
add_test(NAME external_project
         COMMAND ${CMAKE_COMMAND}
                 -DEMU_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                 -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/external
                 -DTOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain-mingw32.cmake
                 -DPICOPAD_TOOLCHAIN_DIR=${PICOPAD_TOOLCHAIN_DIR}
                 -DPICOLIBSDK_DIR=${PICOLIBSDK_DIR}
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/external_test.cmake)
set_tests_properties(external_project PROPERTIES LABELS slow TIMEOUT 900)

# Repo hry: 3 s snimek (ne jednobarevny) + quit v 4.5 s, kod 0
foreach(_g T_TINYBERT T_INVADERS G_ANTS G_MULTISET)
    picopad_headless_test(repo_smoke_${_g} ${_g} 0
        ARGS --headless --script ${PICOPAD_HL_DIR}/smoke.txt
        STDOUT "^end reason=quit "
        EXIST smoke.png
        COLORS smoke.png)
    set_tests_properties(repo_smoke_${_g} PROPERTIES LABELS repo)
endforeach()

# ConfigSave() bez flash: zadny pad (hra konci pres ResetToBootLoader)
picopad_headless_test(cfgsave CFGSAVE 0
    ARGS --headless
    STDOUT "^end reason=exit ")
