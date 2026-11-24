# cmake -P: runs EXE with ARGS (;-list) and checks that its exit code is EXPECT
#   -DEXE=<path> -DEXPECT=<code> [-DARGS=a;b;c]
if(NOT DEFINED EXE OR NOT DEFINED EXPECT)
    message(FATAL_ERROR "usage: cmake -DEXE=... -DEXPECT=... [-DARGS=...] -P expect_exit.cmake")
endif()
execute_process(COMMAND "${EXE}" ${ARGS}
                RESULT_VARIABLE _rc
                OUTPUT_VARIABLE _out
                ERROR_VARIABLE _err
                TIMEOUT 15)
message(STATUS "exit code: ${_rc}")
if(_out)
    message(STATUS "stdout:\n${_out}")
endif()
if(_err)
    message(STATUS "stderr:\n${_err}")
endif()
if(NOT "${_rc}" STREQUAL "${EXPECT}")
    message(FATAL_ERROR "${EXE}: exit code '${_rc}', expected ${EXPECT}")
endif()
