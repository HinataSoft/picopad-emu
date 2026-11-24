# cmake -P: build_device.ps1 pro HELLO, pak uf2check.py na vyslednem UF2
#   -DGAME_DIR -DSDK_DIR -DPYTHON -DTOOLS_DIR
file(REMOVE ${GAME_DIR}/HELLO.uf2 ${GAME_DIR}/HELLO.bin)
execute_process(COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                        -File ${TOOLS_DIR}/build_device.ps1
                        -GameDir ${GAME_DIR} -Device picopad10 -SdkDir ${SDK_DIR}
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "build_device: exit ${_rc}\n${_out}\n${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "build_device.ps1 failed")
endif()
execute_process(COMMAND ${PYTHON} -I ${TOOLS_DIR}/uf2check.py ${GAME_DIR}/HELLO.uf2
                RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
message(STATUS "uf2check: exit ${_rc}\n${_out}\n${_err}")
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "uf2check.py failed")
endif()
