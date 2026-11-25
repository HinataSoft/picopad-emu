# cmake -P: an external project (tests/external) includes PicopadEmu.cmake,
# builds HELLO.exe and the exe runs headless.
#   -DEMU_DIR=<picopad-emu repo> -DBUILD_DIR=<dir, emptied at start>
#   -DTOOLCHAIN_FILE=<toolchain-mingw32.cmake> -DGENERATOR=Ninja
if(NOT DEFINED EMU_DIR OR NOT DEFINED BUILD_DIR OR NOT DEFINED TOOLCHAIN_FILE)
    message(FATAL_ERROR "usage: cmake -DEMU_DIR=... -DBUILD_DIR=... -DTOOLCHAIN_FILE=... -P external_test.cmake")
endif()
if(NOT GENERATOR)
    set(GENERATOR Ninja)
endif()

file(REMOVE_RECURSE "${BUILD_DIR}")
file(MAKE_DIRECTORY "${BUILD_DIR}")

execute_process(COMMAND ${CMAKE_COMMAND} -S "${EMU_DIR}/tests/external" -B "${BUILD_DIR}"
                        -G "${GENERATOR}" "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE}"
                        "-DPICOPAD_EMU_DIR=${EMU_DIR}" -DCMAKE_BUILD_TYPE=Debug
                RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "configure of tests/external failed (${_rc})")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build "${BUILD_DIR}" RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "build of tests/external failed (${_rc})")
endif()

if(NOT EXISTS "${BUILD_DIR}/HELLO.exe")
    message(FATAL_ERROR "${BUILD_DIR}/HELLO.exe was not built")
endif()
execute_process(COMMAND "${BUILD_DIR}/HELLO.exe" --headless --max-time 1s
                WORKING_DIRECTORY "${BUILD_DIR}"
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 30)
message(STATUS "HELLO.exe exit code: ${_rc}\n${_out}")
if(NOT "${_rc}" STREQUAL "3")
    message(FATAL_ERROR "HELLO.exe exit code '${_rc}', expected 3\n${_err}")
endif()
