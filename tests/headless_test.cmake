# cmake -P: runs a game exe (usually --headless) in its own clean directory
# and checks exit code, stdout/stderr lines and written files.
#   -DEXE=<exe> -DEXPECT=<exit code> -DWORK_DIR=<dir, emptied at start>
#   [-DARGS=a;b;c]            arguments
#   [-DMAKE_FILE=name]        create an (empty) file WORK_DIR/name before the run
#   [-DSTDOUT_MATCH=re;re]    each regex must match some stdout line
#   [-DSTDERR_MATCH=re;re]    each regex must match stderr
#   [-DFILES_EXIST=f;f]       files (relative to WORK_DIR) that must exist
#   [-DFILES_ABSENT=f;f]      files that must not exist
#   [-DCOMPARE=f;ref;f;ref]   pairs: file equal byte for byte to ref
#   [-DWAV=f]                 WAV header sizes valid and sample count within
#                             one tick (221 samples) of t*44.1 from the end line
#   [-DMAX_SECONDS=n]         the run must take at most n seconds of real time
if(NOT DEFINED EXE OR NOT DEFINED EXPECT OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "usage: cmake -DEXE=... -DEXPECT=... -DWORK_DIR=... [...] -P headless_test.cmake")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
if(MAKE_FILE)
    file(WRITE "${WORK_DIR}/${MAKE_FILE}" "")
endif()

string(TIMESTAMP _t0 "%s")
execute_process(COMMAND "${EXE}" ${ARGS}
                WORKING_DIRECTORY "${WORK_DIR}"
                RESULT_VARIABLE _rc
                OUTPUT_VARIABLE _out
                ERROR_VARIABLE _err
                TIMEOUT 30)
string(TIMESTAMP _t1 "%s")
math(EXPR _secs "${_t1} - ${_t0}")
message(STATUS "exit code: ${_rc}, ${_secs} s")
message(STATUS "stdout:\n${_out}")
if(_err)
    message(STATUS "stderr:\n${_err}")
endif()

set(_fail "")
if(MAX_SECONDS AND _secs GREATER MAX_SECONDS)
    string(APPEND _fail "run took ${_secs} s, expected at most ${MAX_SECONDS} s
")
endif()
if(NOT "${_rc}" STREQUAL "${EXPECT}")
    string(APPEND _fail "exit code '${_rc}', expected ${EXPECT}\n")
endif()

# stdout as a list of lines (CR of CRLF removed)
string(REPLACE "\r" "" _out "${_out}")
string(REPLACE ";" "\\;" _out_esc "${_out}")
string(REPLACE "\n" ";" _lines "${_out_esc}")
foreach(_re IN LISTS STDOUT_MATCH)
    set(_hit 0)
    foreach(_l IN LISTS _lines)
        if(_l MATCHES "${_re}")
            set(_hit 1)
            break()
        endif()
    endforeach()
    if(NOT _hit)
        string(APPEND _fail "no stdout line matches '${_re}'\n")
    endif()
endforeach()
foreach(_re IN LISTS STDERR_MATCH)
    if(NOT _err MATCHES "${_re}")
        string(APPEND _fail "stderr does not match '${_re}'\n")
    endif()
endforeach()

foreach(_f IN LISTS FILES_EXIST)
    if(NOT EXISTS "${WORK_DIR}/${_f}")
        string(APPEND _fail "missing file ${_f}\n")
    endif()
endforeach()
foreach(_f IN LISTS FILES_ABSENT)
    if(EXISTS "${WORK_DIR}/${_f}")
        string(APPEND _fail "file ${_f} should not exist\n")
    endif()
endforeach()

list(LENGTH COMPARE _n)
set(_i 0)
while(_i LESS _n)
    math(EXPR _j "${_i} + 1")
    list(GET COMPARE ${_i} _f)
    list(GET COMPARE ${_j} _ref)
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${WORK_DIR}/${_f}" "${_ref}"
                    RESULT_VARIABLE _cmp)
    if(NOT _cmp EQUAL 0)
        string(APPEND _fail "${_f} differs from ${_ref}\n")
    endif()
    math(EXPR _i "${_i} + 2")
endwhile()

# little-endian u32 at offset in file -> var
function(read_u32 file offset var)
    file(READ "${file}" _hex OFFSET ${offset} LIMIT 4 HEX)
    string(SUBSTRING "${_hex}" 0 2 _b0)
    string(SUBSTRING "${_hex}" 2 2 _b1)
    string(SUBSTRING "${_hex}" 4 2 _b2)
    string(SUBSTRING "${_hex}" 6 2 _b3)
    math(EXPR _v "0x${_b3}${_b2}${_b1}${_b0}")
    set(${var} ${_v} PARENT_SCOPE)
endfunction()

if(WAV)
    set(_w "${WORK_DIR}/${WAV}")
    if(NOT EXISTS "${_w}")
        string(APPEND _fail "missing WAV ${WAV}\n")
    else()
        file(SIZE "${_w}" _size)
        read_u32("${_w}" 4 _riff)
        read_u32("${_w}" 40 _data)
        math(EXPR _riff_exp "${_size} - 8")
        math(EXPR _data_exp "${_size} - 44")
        if(NOT _riff EQUAL _riff_exp)
            string(APPEND _fail "WAV RIFF size ${_riff}, expected ${_riff_exp}\n")
        endif()
        if(NOT _data EQUAL _data_exp)
            string(APPEND _fail "WAV data size ${_data}, expected ${_data_exp}\n")
        endif()
        if(NOT _out MATCHES "end reason=[a-z-]+ t=([0-9]+)")
            string(APPEND _fail "no end line with t=\n")
        else()
            math(EXPR _expect "${CMAKE_MATCH_1} * 441 / 10")
            math(EXPR _samples "${_data} / 2")
            math(EXPR _diff "${_samples} - ${_expect}")
            if(_diff LESS 0)
                math(EXPR _diff "0 - ${_diff}")
            endif()
            message(STATUS "WAV: ${_samples} samples, t*44.1 = ${_expect}, diff ${_diff}")
            if(_diff GREATER 221)
                string(APPEND _fail "WAV has ${_samples} samples, expected ${_expect} +- 221\n")
            endif()
        endif()
    endif()
endif()

if(_fail)
    message(FATAL_ERROR "${EXE}:\n${_fail}")
endif()
