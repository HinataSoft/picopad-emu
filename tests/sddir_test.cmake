# cmake -P: --sddir of a game exe (SDSYNC) in headless mode.
#   -DEXE=<SDSYNC.exe> -DWORK_DIR=<dir, emptied at start>
# SDSYNC reads /DATA/IN.TXT, writes /DATA/OUT.TXT = "OK:" + the text, rewrites /DATA/MOD.TXT
# ("CHANGED"), deletes /DATA/DEL.TXT, creates /NEWDIR/NEW.TXT ("NEW"), flushes, draws DONE and
# waits (key A ends it). The directory is synced back on every way the run can end.
if(NOT DEFINED EXE OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "usage: cmake -DEXE=... -DWORK_DIR=... -P sddir_test.cmake")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# run_game(<expected rc> <args...>) -> _out, _err  (headless, in WORK_DIR)
function(run_game expect)
    execute_process(COMMAND "${EXE}" --headless ${ARGN}
                    WORKING_DIRECTORY "${WORK_DIR}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 120)
    if(NOT "${_rc}" STREQUAL "${expect}")
        message(FATAL_ERROR "SDSYNC ${ARGN}: exit code '${_rc}', expected ${expect}\nstdout: ${_out}\nstderr: ${_err}")
    endif()
    set(_out "${_out}" PARENT_SCOPE)
    set(_err "${_err}" PARENT_SCOPE)
endfunction()

# fresh tree <name>; every file gets the same old mtime (2020-01-01 10:00:00 local)
function(make_tree name)
    set(_d "${WORK_DIR}/${name}")
    file(REMOVE_RECURSE "${_d}")
    file(MAKE_DIRECTORY "${_d}/DATA")
    file(WRITE "${_d}/DATA/IN.TXT" "HELLO SD")
    file(WRITE "${_d}/DATA/MOD.TXT" "OLD")
    file(WRITE "${_d}/DATA/DEL.TXT" "X")
    file(WRITE "${_d}/DATA/KEEP.TXT" "KEEP")
    file(WRITE "${_d}/Long Name.txt" "L")
    execute_process(COMMAND powershell -NoProfile -Command
        "Get-ChildItem -Recurse -File '${_d}' | ForEach-Object { $_.LastWriteTime = [datetime]'2020-01-01 10:00:00' }"
        RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_VARIABLE _perr)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "cannot set the mtime of the test tree: ${_perr}")
    endif()
endfunction()

function(expect_file path text)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${path} does not exist")
    endif()
    file(READ "${path}" _txt)
    if(NOT "${_txt}" STREQUAL "${text}")
        message(FATAL_ERROR "${path} is '${_txt}', expected '${text}'")
    endif()
endfunction()

# the expected state after a run of SDSYNC (tree <name>)
function(expect_synced name what)
    set(_d "${WORK_DIR}/${name}")
    expect_file("${_d}/DATA/OUT.TXT" "OK:HELLO SD")
    expect_file("${_d}/DATA/MOD.TXT" "CHANGED")
    expect_file("${_d}/NEWDIR/NEW.TXT" "NEW")
    expect_file("${_d}/DATA/IN.TXT" "HELLO SD")
    expect_file("${_d}/DATA/KEEP.TXT" "KEEP")
    expect_file("${_d}/Long Name.txt" "L")
    if(EXISTS "${_d}/DATA/DEL.TXT")
        message(FATAL_ERROR "${what}: DATA/DEL.TXT was not deleted")
    endif()
    # untouched files keep their mtime
    foreach(_f "DATA/KEEP.TXT" "DATA/IN.TXT" "Long Name.txt")
        file(TIMESTAMP "${_d}/${_f}" _ts "%Y-%m-%dT%H:%M:%S")
        if(NOT _ts STREQUAL "2020-01-01T10:00:00")
            message(FATAL_ERROR "${what}: ${_f} was rewritten (mtime ${_ts})")
        endif()
    endforeach()
    # the file outside 8.3 is reported on stderr (run_game's _err)
    if(NOT _err MATCHES "--sddir:" OR NOT _err MATCHES "Long Name\.txt")
        message(FATAL_ERROR "${what}: no warning about 'Long Name.txt' on stderr:\n${_err}")
    endif()
endfunction()

file(WRITE "${WORK_DIR}/press.txt" "1000 press A\n1100 release A\n")
file(WRITE "${WORK_DIR}/quit.txt" "1000 quit\n")

# (a) the game ends itself (key A -> ResetToBootLoader): exit 0, directory synced
make_tree(a)
run_game(0 --sddir "${WORK_DIR}/a" --script "${WORK_DIR}/press.txt")
if(NOT _out MATCHES "end reason=exit ")
    message(FATAL_ERROR "(a): the game did not end itself:\n${_out}")
endif()
expect_synced(a "(a) exit")

# (b) script quit while the game still waits
make_tree(b)
run_game(0 --sddir "${WORK_DIR}/b" --script "${WORK_DIR}/quit.txt")
if(NOT _out MATCHES "end reason=quit ")
    message(FATAL_ERROR "(b): expected end reason=quit:\n${_out}")
endif()
expect_synced(b "(b) quit")

# (c) --max-time with the game still running: exit 3, directory synced
make_tree(c)
run_game(3 --sddir "${WORK_DIR}/c" --max-time 1s)
if(NOT _out MATCHES "end reason=max-time ")
    message(FATAL_ERROR "(c): expected end reason=max-time:\n${_out}")
endif()
expect_synced(c "(c) max-time")

# (d) argument errors: exit 2, the game does not start
make_tree(d)
run_game(2 --sd "${WORK_DIR}/x.img" --sddir "${WORK_DIR}/d")
if(NOT _err MATCHES "--sddir")
    message(FATAL_ERROR "(d) --sd with --sddir: unclear message:\n${_err}")
endif()
run_game(2 --sddir "${WORK_DIR}/does-not-exist")
if(NOT _err MATCHES "--sddir" OR NOT _err MATCHES "does-not-exist")
    message(FATAL_ERROR "(d) missing directory: unclear message:\n${_err}")
endif()
if(_out MATCHES "start game=")
    message(FATAL_ERROR "(d) the game started with an invalid --sddir:\n${_out}")
endif()
# an untouched tree after the failed runs
expect_file("${WORK_DIR}/d/DATA/MOD.TXT" "OLD")
expect_file("${WORK_DIR}/d/DATA/DEL.TXT" "X")
message(STATUS "sddir test OK")
