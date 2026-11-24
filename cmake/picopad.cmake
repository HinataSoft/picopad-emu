# ----------------------------------------------------------------------------
# Preklad prenositelnych knihoven PicoLibSDK pro host (i686 MinGW).
#
# picopad_sdk_objects(<target> <config_dir>)
#   Vytvori OBJECT knihovnu <target> se zdroji SDK vyjmenovanymi nize.
#   <config_dir> (relativne k CMAKE_SOURCE_DIR nebo absolutne) obsahuje
#   config.h a include.h hry; je pridan do include cest spolu s PICOLIBSDK_DIR.
#   Kazda TU dostane -include host/host_prelude.h, ktery nahradi hardwarove
#   hlavicky SDK (sdk_cpu.h, sdk_timer.h, ...) hostitelskymi verzemi.
#
#   Rozsireni (Task 3+): dalsi zdroje (host implementace, hra) se pridavaji
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
get_filename_component(PICOPAD_SDK_ABS "${PICOLIBSDK_DIR}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")

# Prenositelne zdroje SDK (relativne k PICOLIBSDK_DIR)
set(PICOPAD_SDK_SOURCES
    _font/font.c
    _lib/src/lib_calendar.c
    _lib/src/lib_calendar64.c
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

# Volby prekladu kodu SDK (viz Global Constraints)
function(picopad_sdk_compile_options target config_dir)
    get_filename_component(_cfg "${config_dir}" ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
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
        USE_CONFIG=0
        USE_CANVAS=0
        DRAW_HWINTER=0
        # global.h definuje uintptr_t/intptr_t jako (unsigned) long; MinGW crtdefs
        # je nesmi definovat znovu jako (unsigned) int
        _UINTPTR_T_DEFINED
        _INTPTR_T_DEFINED
    )
    target_compile_options(${target} PRIVATE
        "SHELL:-include \"${PICOPAD_HOST_DIR}/host_prelude.h\""
        -funsigned-char
        -fno-strict-aliasing
        $<$<COMPILE_LANGUAGE:C>:-std=gnu11>
        $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17 -fno-exceptions -fno-rtti>
    )
endfunction()

function(picopad_sdk_objects target config_dir)
    set(_srcs)
    foreach(_s IN LISTS PICOPAD_SDK_SOURCES)
        list(APPEND _srcs "${PICOPAD_SDK_ABS}/${_s}")
    endforeach()
    add_library(${target} OBJECT ${_srcs})
    set_target_properties(${target} PROPERTIES C_EXTENSIONS ON CXX_EXTENSIONS ON)
    picopad_sdk_compile_options(${target} "${config_dir}")
endfunction()
