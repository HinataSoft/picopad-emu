# cmake -P: determinism acceptance test of a game under --headless.
#   -DEXE=<game exe> -DWORK_DIR=<dir, emptied at start> -DSCRIPT=<script file>
#   -DREF_DIR=<dir with reference PNGs> -DPREFIX=<ref name prefix, e.g. tetris>
#   -DSHOTS=a;b;c            shot names written by the script
#   -DSEED_SHOTS=x;y         shots (subset of SHOTS) of which at least one must
#                            differ between --seed 1 and --seed 2
# Runs: seed 1 into run1/, seed 1 into run2/, seed 2 into seed2/.
# Checks: exit code 0, "end reason=quit ... code=0 stalls=0", run1 == run2 byte
# for byte, both equal to REF_DIR/<PREFIX>_<shot>.png, seed 2 differs.
foreach(_v EXE WORK_DIR SCRIPT REF_DIR PREFIX SHOTS SEED_SHOTS)
    if(NOT DEFINED ${_v})
        message(FATAL_ERROR "usage: cmake -DEXE=... -DWORK_DIR=... -DSCRIPT=... -DREF_DIR=... -DPREFIX=... -DSHOTS=... -DSEED_SHOTS=... -P determinism_test.cmake")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(_fail "")

function(run_game dir seed)
    file(MAKE_DIRECTORY "${WORK_DIR}/${dir}")
    execute_process(COMMAND "${EXE}" --headless --seed ${seed} --script "${SCRIPT}" --shot-dir "${WORK_DIR}/${dir}"
                    WORKING_DIRECTORY "${WORK_DIR}"
                    RESULT_VARIABLE _rc
                    OUTPUT_VARIABLE _out
                    ERROR_VARIABLE _err
                    TIMEOUT 35)
    message(STATUS "run ${dir} (seed ${seed}): exit code ${_rc}\n${_out}")
    set(_msg "")
    if(NOT "${_rc}" STREQUAL "0")
        string(APPEND _msg "run ${dir}: exit code '${_rc}', expected 0\n")
    endif()
    if(NOT _out MATCHES "end reason=quit t=[0-9]+ code=0 stalls=0 ")
        string(APPEND _msg "run ${dir}: no 'end reason=quit ... code=0 stalls=0' line (stderr: ${_err})\n")
    endif()
    set(_run_msg "${_msg}" PARENT_SCOPE)
endfunction()

run_game(run1 1)
string(APPEND _fail "${_run_msg}")
run_game(run2 1)
string(APPEND _fail "${_run_msg}")
run_game(seed2 2)
string(APPEND _fail "${_run_msg}")

set(_refresh "")
foreach(_s IN LISTS SHOTS)
    set(_f1 "${WORK_DIR}/run1/${_s}.png")
    set(_f2 "${WORK_DIR}/run2/${_s}.png")
    set(_ref "${REF_DIR}/${PREFIX}_${_s}.png")
    if(NOT EXISTS "${_f1}" OR NOT EXISTS "${_f2}")
        string(APPEND _fail "shot ${_s}.png missing in a run\n")
        continue()
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${_f1}" "${_f2}" RESULT_VARIABLE _c)
    if(NOT _c EQUAL 0)
        string(APPEND _fail "NOT DETERMINISTIC: ${_s}.png differs between run1 and run2\n")
    endif()
    if(NOT EXISTS "${_ref}")
        string(APPEND _fail "reference missing: ${_ref}\n")
        string(APPEND _refresh "  ${CMAKE_COMMAND} -E copy \"${_f1}\" \"${_ref}\"\n")
    else()
        execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${_f1}" "${_ref}" RESULT_VARIABLE _c)
        if(NOT _c EQUAL 0)
            string(APPEND _fail "DIFFERS FROM REFERENCE: ${_s}.png (${_f1} vs ${_ref})\n")
            string(APPEND _refresh "  ${CMAKE_COMMAND} -E copy \"${_f1}\" \"${_ref}\"\n")
        endif()
    endif()
endforeach()

set(_seed_differs 0)
foreach(_s IN LISTS SEED_SHOTS)
    if(EXISTS "${WORK_DIR}/seed2/${_s}.png" AND EXISTS "${WORK_DIR}/run1/${_s}.png")
        execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${WORK_DIR}/run1/${_s}.png" "${WORK_DIR}/seed2/${_s}.png" RESULT_VARIABLE _c)
        if(NOT _c EQUAL 0)
            set(_seed_differs 1)
        endif()
    endif()
endforeach()
if(NOT _seed_differs)
    string(APPEND _fail "seed has no effect: no shot of '${SEED_SHOTS}' differs between --seed 1 and --seed 2\n")
endif()

if(_refresh)
    string(APPEND _fail "To refresh the reference (if the change is intended), run:\n${_refresh}")
endif()
if(_fail)
    message(FATAL_ERROR "${EXE}:\n${_fail}")
endif()
