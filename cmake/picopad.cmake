# ----------------------------------------------------------------------------
# Preklad prenositelnych knihoven PicoLibSDK pro host (i686 MinGW).
#
# picopad_sdk_objects(<target> <config_dir>)
#   Vytvori OBJECT knihovnu <target> se zdroji SDK vyjmenovanymi nize.
#   <config_dir> (relativne k CMAKE_CURRENT_SOURCE_DIR volajiciho nebo absolutne) obsahuje
#   config.h a include.h hry; je pridan do include cest spolu s PICOLIBSDK_DIR.
#   Obsahuje i hostitelske zdroje PICOPAD_HOST_SOURCES (host/*.c, heap.S).
#   Kazda C/C++ TU dostane -include host/host_prelude.h, ktery nahradi hardwarove
#   hlavicky SDK (sdk_cpu.h, sdk_timer.h, ...) hostitelskymi verzemi.
#
#   Rozsireni: dalsi zdroje (host implementace, hra) se pridavaji
#   pres  target_sources(<target> PRIVATE ...)  - dostanou stejne volby,
#   protoze vsechny volby jsou PRIVATE vlastnosti teto OBJECT knihovny.
#   Objekty se pouziji v exe pres $<TARGET_OBJECTS:<target>> nebo
#   target_link_libraries(<exe> PRIVATE <target>).
#   Volby prekladu jsou k dispozici i samostatne funkci
#   picopad_sdk_compile_options(<target> <config_dir>), aby je mohl pouzit
#   jiny cil (napr. exe s herni main()).
#
# SDK (PICOLIBSDK_DIR) se nemeni a do jeho stromu se nic nezapisuje.
# ----------------------------------------------------------------------------

set(PICOPAD_HOST_DIR "${CMAKE_CURRENT_LIST_DIR}/../host")
get_filename_component(PICOPAD_HOST_DIR "${PICOPAD_HOST_DIR}" ABSOLUTE)
get_filename_component(PICOPAD_SDK_ABS "${PICOLIBSDK_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

# Prenositelne zdroje SDK (relativne k PICOLIBSDK_DIR)
set(PICOPAD_SDK_SOURCES
    _font/font.c
    _lib/src/lib_calendar.c
    _lib/src/lib_calendar64.c
    _lib/src/lib_config.c
    _lib/src/lib_color.c
    _lib/src/lib_crc.c
    _lib/src/lib_decnum.c
    _lib/src/lib_draw.c
    _lib/src/lib_escpkt.c
    _lib/src/lib_fat.c
    _lib/src/lib_list.c
    _lib/src/lib_mat2d.c
    _lib/src/lib_print.c
    _lib/src/lib_pwmsnd.c
    _lib/src/lib_rand.c
    _lib/src/lib_rect.c
    _lib/src/lib_stream.c
    _lib/src/lib_text.c
    _lib/src/lib_textlist.c
    _lib/src/lib_textnum.c
    _lib/src/lib_tprint.c
    _lib/src/lib_tree.c
    _lib/src/lib_alarm.c
    _lib/src/lib_event.c
    _lib/src/lib_miniring.c
    _lib/src/lib_ring.c
    _lib/src/lib_malloc.c
    _devices/picopad/picopad_init.c
    _devices/picopad/picopad_key.c
    _devices/picopad/picopad_led.c
    _devices/picopad/picopad_ss.c
)

# Hostitelska implementace (relativne k PICOPAD_HOST_DIR): hodiny, pumpa
# SysTicku, runtime, stuby SDK. Pridava se do kazde picopad_sdk_objects.
# host/game_entry.cpp sem nepatri - pridava ho az cil hry (vola C++ game_main).
# disp.c (displej), keys.c (klavesy pres GPIO) a audio.c (PWM zvuk, mixer).
set(PICOPAD_HOST_SOURCES
    clock.c
    pump.c
    runtime.c
    sdk_stubs.c
    sd.c
    bat.c
    heap.S
    disp.c
    keys.c
    audio.c
    config.c
    libc_compat.c
)

# lib_fat.c: CheckApp() cte hlavicku aplikace primo z flash
# (XIP_BASE + BOOTLOADER_SIZE), coz na hostu spadne. Bez zmeny SDK to nejde
# obejit, proto se v teto jedine TU prejmenuji CheckApp, CheckAppPath
# a GetHomePath (volaji se navzajem) a host/sdk_stubs.c definuje nahrady,
# ktere hlasi "zadna aplikace". CheckLoaderData zustava (cte LoaderData[]).
set_source_files_properties("${PICOPAD_SDK_ABS}/_lib/src/lib_fat.c" PROPERTIES
    COMPILE_DEFINITIONS "CheckApp=sdk_unused_CheckApp;CheckAppPath=sdk_unused_CheckAppPath;GetHomePath=sdk_unused_GetHomePath")

# lib_config.c: ConfigLoad/ConfigSave/ConfigClear pracuji s flash (sektor
# CONFIG_FLASH_OFF), coz na hostu nejde. V teto jedine TU se prejmenuji
# (sdk_unused_*) a host/config.c definuje nahrady (konfigurace jen v pameti,
# ConfigLoad = vychozi hodnoty). Zbytek (Config, ConfigSetDef, ConfigSetVolume)
# zustava puvodni.
set_source_files_properties("${PICOPAD_SDK_ABS}/_lib/src/lib_config.c" PROPERTIES
    COMPILE_DEFINITIONS "ConfigLoad=sdk_unused_ConfigLoad;ConfigSave=sdk_unused_ConfigSave;ConfigClear=sdk_unused_ConfigClear")

# Volby prekladu kodu SDK (viz Global Constraints)
function(picopad_sdk_compile_options target config_dir)
    get_filename_component(_cfg "${config_dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(${target} PRIVATE "${_cfg}" "${PICOPAD_SDK_ABS}")
    target_compile_definitions(${target} PRIVATE
        RP2040=1
        USE_PICOPAD=1
        USE_PICOPAD10=1
        FLASHSIZE=0x200000
        RAMSIZE=0x40000
        "INCLUDES_H=\"${PICOPAD_SDK_ABS}/includes.h\""
        "CONFIG_DEF_H=\"${PICOPAD_SDK_ABS}/config_def.h\""
        USE_FLOATLIBC=1
        USE_DOUBLELIBC=1
        USE_CANVAS=0
        DRAW_HWINTER=0
        # global.h definuje uintptr_t/intptr_t jako (unsigned) long; MinGW crtdefs
        # je nesmi definovat znovu jako (unsigned) int
        _UINTPTR_T_DEFINED
        _INTPTR_T_DEFINED
    )
    # prelude a volby jazyka C jen pro C/C++ (ne pro host/heap.S)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include \"${PICOPAD_HOST_DIR}/host_prelude.h\">"
        $<$<COMPILE_LANGUAGE:C,CXX>:-funsigned-char>
        $<$<COMPILE_LANGUAGE:C,CXX>:-fno-strict-aliasing>
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17 -fno-exceptions -fno-rtti>
    )
endfunction()

function(picopad_sdk_objects target config_dir)
    set(_srcs)
    foreach(_s IN LISTS PICOPAD_SDK_SOURCES)
        list(APPEND _srcs "${PICOPAD_SDK_ABS}/${_s}")
    endforeach()
    foreach(_s IN LISTS PICOPAD_HOST_SOURCES)
        list(APPEND _srcs "${PICOPAD_HOST_DIR}/${_s}")
    endforeach()
    add_library(${target} OBJECT ${_srcs})
    set_target_properties(${target} PROPERTIES C_EXTENSIONS ON CXX_EXTENSIONS ON)
    picopad_sdk_compile_options(${target} "${config_dir}")
endfunction()

# ----------------------------------------------------------------------------
# picopad_exe_runtime(<target> [SDL2])
#   Kazde exe projektu: MinGW runtime (libgcc, libstdc++, libwinpthread) se
#   linkuje staticky (-static), takze exe nepotrebuje zadnou DLL z MinGW
#   (jen systemove DLL Windows a SDL2.dll). SDL2 zustava dynamicka pres svou
#   importni knihovnu. S volbou SDL2 se po sestaveni zkopiruje SDL2.dll vedle exe.
# ----------------------------------------------------------------------------
function(picopad_exe_runtime target)
    target_link_options(${target} PRIVATE -static)
    if("SDL2" IN_LIST ARGN)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:${target}>)
    endif()
endfunction()

# ----------------------------------------------------------------------------
# picopad_add_game(<NAME> <GAME_DIR>)
#   Builds an unchanged PicoLibSDK game into <NAME>.exe (window title <NAME>):
#    - sources = the SRC +=/CSRC += lines of <GAME_DIR>/Makefile (relative to
#      <GAME_DIR>), compiled in OBJECT library <NAME>_game with the SDK options
#      and -Dmain=game_main (only these sources);
#    - SDK + host layer = picopad_sdk_objects(<NAME>_sdk <GAME_DIR>) (the game's
#      own config.h/include.h live in <GAME_DIR>);
#    - host/game_entry.cpp and platform/ (SDL2 window, audio, CLI, headless
#      mode with script, PNG and WAV) in the exe;
#    - links SDL2main + SDL2 (+ winmm) via picopad_exe_runtime(): static MinGW
#      runtime, SDL2.dll copied next to the exe.
#   Writes nothing into <GAME_DIR> or the SDK.
# ----------------------------------------------------------------------------
include("${CMAKE_CURRENT_LIST_DIR}/parse_makefile.cmake")
set(PICOPAD_PLATFORM_DIR "${CMAKE_CURRENT_LIST_DIR}/../platform")
get_filename_component(PICOPAD_PLATFORM_DIR "${PICOPAD_PLATFORM_DIR}" ABSOLUTE)
set(PICOPAD_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include")
get_filename_component(PICOPAD_INCLUDE_DIR "${PICOPAD_INCLUDE_DIR}" ABSOLUTE)

# Pure C platform libraries linked by every game exe (no SDL, no SDK); defined
# here so that external projects get them too. Guarded: including twice is fine.
if(NOT TARGET platform_png)
    add_library(platform_png STATIC "${PICOPAD_PLATFORM_DIR}/png.c")
endif()
if(NOT TARGET platform_wav)
    add_library(platform_wav STATIC "${PICOPAD_PLATFORM_DIR}/wav.c")
endif()
if(NOT TARGET platform_sdimage)
    add_library(platform_sdimage STATIC "${PICOPAD_PLATFORM_DIR}/sdimage.c")
endif()
if(NOT TARGET platform_script)
    add_library(platform_script STATIC "${PICOPAD_PLATFORM_DIR}/script.c")
    target_include_directories(platform_script PUBLIC "${PICOPAD_PLATFORM_DIR}" "${PICOPAD_INCLUDE_DIR}")
endif()

function(picopad_add_game name game_dir)
    get_filename_component(_dir "${game_dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(_mk "${_dir}/Makefile")
    picopad_parse_makefile("${_mk}" _rel _asm)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_mk}")
    if(NOT _rel)
        message(FATAL_ERROR "picopad_add_game(${name}): no SRC/CSRC sources in ${_mk}")
    endif()
    if(_asm)
        message(WARNING "picopad_add_game(${name}): ASRC sources are not supported, ignored: ${_asm}")
    endif()
    set(_srcs)
    foreach(_s IN LISTS _rel)
        list(APPEND _srcs "${_dir}/${_s}")
    endforeach()

    picopad_sdk_objects(${name}_sdk "${_dir}")

    add_library(${name}_game OBJECT ${_srcs})
    set_target_properties(${name}_game PROPERTIES C_EXTENSIONS ON CXX_EXTENSIONS ON)
    picopad_sdk_compile_options(${name}_game "${_dir}")
    target_compile_definitions(${name}_game PRIVATE main=game_main)

    add_executable(${name}
        "${PICOPAD_HOST_DIR}/game_entry.cpp"
        "${PICOPAD_PLATFORM_DIR}/main.cpp"
        "${PICOPAD_PLATFORM_DIR}/cli.c"
        "${PICOPAD_PLATFORM_DIR}/keymap.c"
        "${PICOPAD_PLATFORM_DIR}/headless.c"
        $<TARGET_OBJECTS:${name}_game>
        $<TARGET_OBJECTS:${name}_sdk>
    )
    target_compile_definitions(${name} PRIVATE "PICOPAD_GAME_NAME=\"${name}\"")
    # platform_script/png/wav: pure C libraries defined above in this file
    target_link_libraries(${name} PRIVATE SDL2::SDL2main SDL2::SDL2 winmm
        platform_script platform_png platform_wav platform_sdimage)
    picopad_exe_runtime(${name} SDL2)
endfunction()
