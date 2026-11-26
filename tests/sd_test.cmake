# cmake -P: --sd of a game exe (SDTEST) in headless mode.
#   -DEXE=<SDTEST.exe> -DSDIMG=<picopad-sdimg.exe> -DWORK_DIR=<dir, emptied at start>
#   -DSD_DIR=<tests/sd> -DPICOPAD_HL_DIR=<tests/headless>
# SDTEST reads /TEST/IN.TXT ("HELLO SD"), writes /TEST/OUT.TXT ("OK:HELLO SD"),
# shows the text, waits 2 s and exits (ResetToBootLoader); without a card it
# draws NO CARD and ends the same way.
if(NOT DEFINED EXE OR NOT DEFINED SDIMG OR NOT DEFINED WORK_DIR OR NOT DEFINED SD_DIR)
    message(FATAL_ERROR "usage: cmake -DEXE=... -DSDIMG=... -DWORK_DIR=... -DSD_DIR=... -P sd_test.cmake")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# run_game(<expected rc> <args...>) -> _out, _err  (headless, in WORK_DIR)
function(run_game expect)
    execute_process(COMMAND "${EXE}" --headless ${ARGN}
                    WORKING_DIRECTORY "${WORK_DIR}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 60)
    if(NOT "${_rc}" STREQUAL "${expect}")
        message(FATAL_ERROR "SDTEST ${ARGN}: exit code '${_rc}', expected ${expect}\nstdout: ${_out}\nstderr: ${_err}")
    endif()
    set(_out "${_out}" PARENT_SCOPE)
    set(_err "${_err}" PARENT_SCOPE)
endfunction()

function(run_sdimg expect)
    execute_process(COMMAND "${SDIMG}" ${ARGN}
                    WORKING_DIRECTORY "${WORK_DIR}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 60)
    if(NOT "${_rc}" STREQUAL "${expect}")
        message(FATAL_ERROR "picopad-sdimg ${ARGN}: exit code '${_rc}', expected ${expect}\nstdout: ${_out}\nstderr: ${_err}")
    endif()
    set(_out "${_out}" PARENT_SCOPE)
endfunction()

# OUT.TXT of an image must be exactly "OK:HELLO SD"
function(expect_out img what)
    file(REMOVE "${WORK_DIR}/out.txt")
    run_sdimg(0 get "${img}" /TEST/OUT.TXT "${WORK_DIR}/out.txt")
    file(READ "${WORK_DIR}/out.txt" _txt)
    if(NOT "${_txt}" STREQUAL "OK:HELLO SD")
        message(FATAL_ERROR "${what}: OUT.TXT is '${_txt}', expected 'OK:HELLO SD'")
    endif()
endfunction()

function(fresh_image name)
    file(COPY_FILE "${WORK_DIR}/card.img" "${WORK_DIR}/${name}" ONLY_IF_DIFFERENT)
endfunction()

file(WRITE "${WORK_DIR}/shot.txt" "500 shot s\n6000 quit\n")
file(WRITE "${WORK_DIR}/early_quit.txt" "1000 quit\n")

run_sdimg(0 create "${WORK_DIR}/card.img" --size 8 --from "${SD_DIR}")

# (a) the game ends itself (ResetToBootLoader): exit 0, OUT.TXT in the image
fresh_image(a.img)
run_game(0 --sd "${WORK_DIR}/a.img" --script "${WORK_DIR}/shot.txt" --shot-dir "${WORK_DIR}/shots_a")
if(NOT _out MATCHES "end reason=exit ")
    message(FATAL_ERROR "(a): the game did not end itself:\n${_out}")
endif()
expect_out("${WORK_DIR}/a.img" "(a) exit")
# IN.TXT and the other files survive
run_sdimg(0 get "${WORK_DIR}/a.img" /TEST/IN.TXT "${WORK_DIR}/in.out")
file(READ "${WORK_DIR}/in.out" _in)
if(NOT "${_in}" STREQUAL "HELLO SD")
    message(FATAL_ERROR "(a): IN.TXT is '${_in}'")
endif()
run_sdimg(0 get "${WORK_DIR}/a.img" /README.TXT "${WORK_DIR}/readme.out")

# (b) script quit before the game ends itself, after the write: data in the file
fresh_image(b.img)
run_game(0 --sd "${WORK_DIR}/b.img" --script "${WORK_DIR}/early_quit.txt")
if(NOT _out MATCHES "end reason=quit ")
    message(FATAL_ERROR "(b): expected end reason=quit:\n${_out}")
endif()
expect_out("${WORK_DIR}/b.img" "(b) quit")

# (c) --max-time before the game ends itself: exit 3, data in the file
fresh_image(c.img)
run_game(3 --sd "${WORK_DIR}/c.img" --max-time 1s)
if(NOT _out MATCHES "end reason=max-time ")
    message(FATAL_ERROR "(c): expected end reason=max-time:\n${_out}")
endif()
expect_out("${WORK_DIR}/c.img" "(c) max-time")

# (d) no --sd: no card, the game ends normally, no image appears; the screen
#     differs from the one with a card
run_game(0 --script "${WORK_DIR}/shot.txt" --shot-dir "${WORK_DIR}/shots_d")
if(NOT _out MATCHES "end reason=exit ")
    message(FATAL_ERROR "(d): the game did not end itself:\n${_out}")
endif()
file(GLOB _imgs "${WORK_DIR}/*.img")
list(LENGTH _imgs _n)
if(NOT _n EQUAL 4)   # card, a, b, c only
    message(FATAL_ERROR "(d): unexpected images: ${_imgs}")
endif()
file(SHA256 "${WORK_DIR}/shots_a/s.png" _ha)
file(SHA256 "${WORK_DIR}/shots_d/s.png" _hd)
if(_ha STREQUAL _hd)
    message(FATAL_ERROR "(d): the screen without a card equals the screen with a card")
endif()

# (e) invalid images: exit 2 and a clear message at startup
set(_missing "${WORK_DIR}/missing.img")
run_game(2 --sd "${_missing}")
if(NOT _err MATCHES "--sd" OR NOT _err MATCHES "missing\.img")
    message(FATAL_ERROR "(e) missing: unclear message:\n${_err}")
endif()
string(REPEAT "x" 1000 _x1000)
file(WRITE "${WORK_DIR}/size1000.img" "${_x1000}")
run_game(2 --sd "${WORK_DIR}/size1000.img")
if(NOT _err MATCHES "--sd" OR NOT _err MATCHES "size1000\.img" OR NOT _err MATCHES "512")
    message(FATAL_ERROR "(e) 1000 B: unclear message:\n${_err}")
endif()
string(REPEAT "x" 70000 _x70000)
file(WRITE "${WORK_DIR}/size70000.img" "${_x70000}")   # over 64 KiB, not a multiple of 512
run_game(2 --sd "${WORK_DIR}/size70000.img")
if(NOT _err MATCHES "512")
    message(FATAL_ERROR "(e) 70000 B: unclear message:\n${_err}")
endif()
string(REPEAT "x" 32768 _x32k)
file(WRITE "${WORK_DIR}/size32k.img" "${_x32k}")        # multiple of 512 but below 64 KiB
run_game(2 --sd "${WORK_DIR}/size32k.img")
if(NOT _err MATCHES "--sd" OR NOT _err MATCHES "size32k\.img" OR NOT _err MATCHES "64 KiB")
    message(FATAL_ERROR "(e) 32 KiB: unclear message:\n${_err}")
endif()
file(WRITE "${WORK_DIR}/size0.img" "")
run_game(2 --sd "${WORK_DIR}/size0.img")
file(MAKE_DIRECTORY "${WORK_DIR}/dir.img")
run_game(2 --sd "${WORK_DIR}/dir.img")
if(NOT _err MATCHES "--sd" OR NOT _err MATCHES "directory")
    message(FATAL_ERROR "(e) directory: unclear message:\n${_err}")
endif()
# an invalid --sd must not start the game
if(_out MATCHES "start game=")
    message(FATAL_ERROR "(e) the game started with an invalid image:\n${_out}")
endif()

# (f) read-only image file: opens, writes fail (the game shows WRITE ERR and
#     ends normally), exit 0, the image is unchanged and has no OUT.TXT
fresh_image(f.img)
file(SHA256 "${WORK_DIR}/f.img" _before)
execute_process(COMMAND attrib +R "${WORK_DIR}/f.img" RESULT_VARIABLE _rc OUTPUT_QUIET ERROR_QUIET)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "attrib +R failed")
endif()
run_game(0 --sd "${WORK_DIR}/f.img" --script "${WORK_DIR}/shot.txt" --shot-dir "${WORK_DIR}/shots_f")
if(NOT _out MATCHES "end reason=exit ")
    message(FATAL_ERROR "(f): the game did not end normally:\n${_out}")
endif()
file(SHA256 "${WORK_DIR}/shots_f/s.png" _hf)
if(_hf STREQUAL _ha OR _hf STREQUAL _hd)
    message(FATAL_ERROR "(f): the screen is neither WRITE ERR (differs from OK and NO CARD screens)")
endif()
file(SHA256 "${WORK_DIR}/f.img" _after)
execute_process(COMMAND attrib -R "${WORK_DIR}/f.img")
if(NOT _before STREQUAL _after)
    message(FATAL_ERROR "(f): the read-only image changed")
endif()
run_sdimg(1 get "${WORK_DIR}/f.img" /TEST/OUT.TXT "${WORK_DIR}/f.out")
message(STATUS "sd test OK")
