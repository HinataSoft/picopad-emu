# cmake -P test of cmake/parse_makefile.cmake
#   -DWORK_DIR=<dir for temporary files> (default: current directory)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/parse_makefile.cmake")
if(NOT WORK_DIR)
    set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}")
endif()

set(_fail 0)
macro(expect_list actual expected what)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(SEND_ERROR "${what}:\n  got      '${actual}'\n  expected '${expected}'")
        set(_fail 1)
    endif()
endmacro()

# 1. copy of the TETRIS Makefile (PicoPad/GAME/TETRIS/Makefile)
picopad_parse_makefile("${CMAKE_CURRENT_LIST_DIR}/data/Makefile.tetris" _src _asm)
expect_list("${_src}"
    "src/main.cpp;img/tiles.cpp;snd/brick.cpp;snd/gameover.cpp;snd/levelup.cpp;snd/line.cpp"
    "TETRIS sources")
expect_list("${_asm}" "" "TETRIS asm sources")

# 2. synthetic Makefile: comments, CSRC, no spaces, tabs, continuation, ASRC
set(_mk "${WORK_DIR}/parse_makefile_test.mk")
file(WRITE "${_mk}" [[
# comment SRC += no.cpp
CSRC += a.c b.c   # trailing comment
SRC+=x/y.cpp
	SRC	+=	tab.cpp
SRC += cont1.cpp \
	cont2.cpp
ASRC += s.S
MYSRC += not.cpp
SRC =
SRC +=
PICO_ROOT_PATH = ../../..
]])
picopad_parse_makefile("${_mk}" _src _asm)
expect_list("${_src}" "a.c;b.c;x/y.cpp;tab.cpp;cont1.cpp;cont2.cpp" "synthetic sources")
expect_list("${_asm}" "s.S" "synthetic asm sources")
file(REMOVE "${_mk}")

# 3. the asm output is optional
picopad_parse_makefile("${CMAKE_CURRENT_LIST_DIR}/data/Makefile.tetris" _only)
list(LENGTH _only _n)
expect_list("${_n}" "6" "TETRIS source count (2-argument form)")

if(_fail)
    message(FATAL_ERROR "parse_makefile test FAILED")
endif()
message(STATUS "parse_makefile test passed")
