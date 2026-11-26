# cmake -P: picopad-sdimg creates, lists, reads and writes FAT images.
#   -DEXE=<picopad-sdimg> -DWORK_DIR=<dir, emptied at start> -DSD_DIR=<tests/sd>
#   -DPYTHON=<python> (for the 4 KiB random file)
if(NOT DEFINED EXE OR NOT DEFINED WORK_DIR OR NOT DEFINED SD_DIR OR NOT DEFINED PYTHON)
    message(FATAL_ERROR "usage: cmake -DEXE=... -DWORK_DIR=... -DSD_DIR=... -DPYTHON=... -P sdimg_test.cmake")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# run_sdimg(<expected rc> <args...>) -> _out, _err
function(run_sdimg expect)
    execute_process(COMMAND "${EXE}" ${ARGN}
                    WORKING_DIRECTORY "${WORK_DIR}"
                    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 60)
    if(NOT "${_rc}" STREQUAL "${expect}")
        message(FATAL_ERROR "picopad-sdimg ${ARGN}: exit code '${_rc}', expected ${expect}\nstdout: ${_out}\nstderr: ${_err}")
    endif()
    set(_out "${_out}" PARENT_SCOPE)
    set(_err "${_err}" PARENT_SCOPE)
endfunction()

function(expect_same a b what)
    file(SHA256 "${a}" _ha)
    file(SHA256 "${b}" _hb)
    if(NOT _ha STREQUAL _hb)
        message(FATAL_ERROR "${what}: ${a} differs from ${b}")
    endif()
endfunction()

# 1. create from tests/sd, ls
run_sdimg(0 create "${WORK_DIR}/card.img" --size 8 --from "${SD_DIR}")
run_sdimg(0 ls "${WORK_DIR}/card.img" /)
if(NOT _out MATCHES "(^|\n)D TEST\r?\n" OR NOT _out MATCHES "(^|\n)F README.TXT [0-9]+\r?\n")
    message(FATAL_ERROR "ls / lacks 'D TEST' or 'F README.TXT <size>':\n${_out}")
endif()
run_sdimg(0 ls "${WORK_DIR}/card.img" /TEST)
if(NOT _out MATCHES "(^|\n)F IN.TXT 8\r?\n")
    message(FATAL_ERROR "ls /TEST lacks 'F IN.TXT 8':\n${_out}")
endif()
file(SIZE "${WORK_DIR}/card.img" _isz)
if(NOT _isz EQUAL 8388608)
    message(FATAL_ERROR "image size ${_isz}, expected 8388608")
endif()

# 2. get returns the original bytes
run_sdimg(0 get "${WORK_DIR}/card.img" /TEST/IN.TXT "${WORK_DIR}/in.out")
expect_same("${SD_DIR}/TEST/IN.TXT" "${WORK_DIR}/in.out" "get /TEST/IN.TXT")

# 3. put + get of 4 KiB random bytes (new file, then overwrite with another)
file(WRITE "${WORK_DIR}/rnd.py" "
import random, sys
random.seed(int(sys.argv[2]))
open(sys.argv[1], 'wb').write(bytes(random.getrandbits(8) for _ in range(4096)))
")
foreach(_seed 1 2)
    execute_process(COMMAND "${PYTHON}" -I "${WORK_DIR}/rnd.py" "${WORK_DIR}/rnd${_seed}.bin" ${_seed}
                    RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "random file generation failed")
    endif()
endforeach()
run_sdimg(0 put "${WORK_DIR}/card.img" /TEST/RND.BIN "${WORK_DIR}/rnd1.bin")
run_sdimg(0 get "${WORK_DIR}/card.img" /TEST/RND.BIN "${WORK_DIR}/rnd1.out")
expect_same("${WORK_DIR}/rnd1.bin" "${WORK_DIR}/rnd1.out" "put+get")
run_sdimg(0 put "${WORK_DIR}/card.img" /TEST/RND.BIN "${WORK_DIR}/rnd2.bin")
run_sdimg(0 get "${WORK_DIR}/card.img" /TEST/RND.BIN "${WORK_DIR}/rnd2.out")
expect_same("${WORK_DIR}/rnd2.bin" "${WORK_DIR}/rnd2.out" "overwrite put+get")
run_sdimg(0 ls "${WORK_DIR}/card.img" /TEST)
if(NOT _out MATCHES "(^|\n)F RND.BIN 4096\r?\n" OR NOT _out MATCHES "F IN.TXT 8")
    message(FATAL_ERROR "ls /TEST after put:\n${_out}")
endif()
# an untouched file survives the writes; a missing file is an operation error
run_sdimg(0 get "${WORK_DIR}/card.img" /README.TXT "${WORK_DIR}/readme.out")
expect_same("${SD_DIR}/README.TXT" "${WORK_DIR}/readme.out" "get /README.TXT")
run_sdimg(1 get "${WORK_DIR}/card.img" /NOPE.TXT "${WORK_DIR}/nope.out")

# 4. long file name: lib_fat has no LFN, so create must exit 1 naming the file
#    (or store it and list it); exit 0 without the file is a failure
file(MAKE_DIRECTORY "${WORK_DIR}/longsrc")
file(WRITE "${WORK_DIR}/longsrc/dlouhe-jmeno-souboru.txt" "long")
execute_process(COMMAND "${EXE}" create "${WORK_DIR}/long.img" --size 8 --from "${WORK_DIR}/longsrc"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 60)
if(_rc EQUAL 0)
    run_sdimg(0 ls "${WORK_DIR}/long.img" /)
    if(NOT _out MATCHES "dlouhe-jmeno-souboru.txt")
        message(FATAL_ERROR "create exited 0 but the long-named file is not in the image:\n${_out}")
    endif()
elseif(_rc EQUAL 1)
    if(NOT "${_out}${_err}" MATCHES "dlouhe-jmeno-souboru.txt")
        message(FATAL_ERROR "create exited 1 without naming the file:\n${_out}${_err}")
    endif()
else()
    message(FATAL_ERROR "create with long name: exit code ${_rc}, expected 0 or 1")
endif()
message(STATUS "long file name create: exit code ${_rc}")

# 5. invalid arguments -> 2
run_sdimg(2)
run_sdimg(2 frobnicate "${WORK_DIR}/card.img")
run_sdimg(2 create)
run_sdimg(2 create "${WORK_DIR}/x.img" --size)
run_sdimg(2 create "${WORK_DIR}/x.img" --size 0)
run_sdimg(2 create "${WORK_DIR}/x.img" --size abc)
run_sdimg(2 create "${WORK_DIR}/x.img" --bogus)
run_sdimg(2 ls)
run_sdimg(2 get "${WORK_DIR}/card.img" /TEST/IN.TXT)
run_sdimg(2 put "${WORK_DIR}/card.img" /X.TXT)
run_sdimg(2 ls "${WORK_DIR}/card.img" / extra)
# unopenable image: operation error (1), not a crash
run_sdimg(1 ls "${WORK_DIR}/missing.img" /)
message(STATUS "sdimg test OK")
