# ----------------------------------------------------------------------------
# picopad_parse_makefile(<makefile> <out_src> [<out_asm>])
#   Reads the source list of a PicoLibSDK game Makefile: the words of all
#   "SRC += ..." and "CSRC += ..." lines (C++ and C, in file order) go to
#   <out_src>, the words of "ASRC += ..." lines to <out_asm>. Paths are returned
#   as written (relative to the Makefile's directory). Comments (#...) and
#   backslash line continuations are handled; other variables are ignored.
#   Usable from a project and from cmake -P scripts.
# ----------------------------------------------------------------------------
function(picopad_parse_makefile makefile out_src)
    if(NOT EXISTS "${makefile}")
        message(FATAL_ERROR "picopad_parse_makefile: '${makefile}' not found")
    endif()
    file(READ "${makefile}" _text)
    # one logical line per list element; ';' and '[' ']' would confuse lists
    string(REPLACE "\r" "" _text "${_text}")
    string(REPLACE ";" " " _text "${_text}")
    string(REPLACE "\\\n" " " _text "${_text}")   # line continuation
    string(REPLACE "\n" ";" _lines "${_text}")

    set(_src)
    set(_asm)
    foreach(_line IN LISTS _lines)
        string(REGEX REPLACE "#.*$" "" _line "${_line}")
        if(_line MATCHES "^[ \t]*(A?C?SRC)[ \t]*\\+=(.*)$")
            set(_var "${CMAKE_MATCH_1}")
            string(REGEX MATCHALL "[^ \t]+" _words "${CMAKE_MATCH_2}")
            if(_var STREQUAL "ASRC")
                list(APPEND _asm ${_words})
            elseif(_var STREQUAL "SRC" OR _var STREQUAL "CSRC")
                list(APPEND _src ${_words})
            endif()
        endif()
    endforeach()

    set(${out_src} "${_src}" PARENT_SCOPE)
    if(ARGC GREATER 2)
        set(${ARGV2} "${_asm}" PARENT_SCOPE)
    endif()
endfunction()
