# Repo coverage: an external project (like tests/external) that adds every
# PicoPad/GAME/* and PicoPad/TINYGAME/* directory of PicoLibSDK having a Makefile
# as G_<DIR> / T_<DIR>. Driven by tools/coverage/run.cmake.
#   -DPICOPAD_EMU_DIR=<picopad-emu> [-DPICOLIBSDK_DIR=...] [-DPICOPAD_TOOLCHAIN_DIR=...]
cmake_minimum_required(VERSION 3.21)
project(picopad_coverage C CXX ASM)
include(${PICOPAD_EMU_DIR}/cmake/PicopadEmu.cmake)

set(_names)
foreach(_kind IN ITEMS GAME TINYGAME)
    if(_kind STREQUAL "GAME")
        set(_prefix G_)
    else()
        set(_prefix T_)
    endif()
    file(GLOB _dirs LIST_DIRECTORIES true "${PICOLIBSDK_DIR}/PicoPad/${_kind}/*")
    list(SORT _dirs)
    foreach(_d IN LISTS _dirs)
        if(IS_DIRECTORY "${_d}" AND EXISTS "${_d}/Makefile")
            get_filename_component(_n "${_d}" NAME)
            picopad_add_game(${_prefix}${_n} "${_d}")
            list(APPEND _names ${_prefix}${_n})
        endif()
    endforeach()
endforeach()
# the list of games for the runner (build/games.txt)
string(REPLACE ";" "\n" _txt "${_names}")
file(WRITE "${CMAKE_BINARY_DIR}/games.txt" "${_txt}\n")
