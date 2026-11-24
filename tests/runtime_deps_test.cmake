# Spusti EXE s PATH jen na systemove adresare Windows: exe nesmi potrebovat
# zadnou DLL z MinGW (libwinpthread-1.dll, libgcc_s, libstdc++), jen systemove
# DLL a pripadne SDL2.dll vedle exe. Ocekava navratovy kod 0.
#   cmake -DEXE=<exe> -P runtime_deps_test.cmake
if(NOT EXE)
    message(FATAL_ERROR "runtime_deps_test: chybi -DEXE=<exe>")
endif()
set(_sysroot "$ENV{SystemRoot}")
if(NOT _sysroot)
    set(_sysroot "C:/Windows")
endif()
file(TO_CMAKE_PATH "${_sysroot}" _sysroot)
set(ENV{PATH} "${_sysroot}/System32;${_sysroot}")
execute_process(COMMAND "${EXE}" RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err TIMEOUT 30)
if(NOT _rc STREQUAL "0")
    message(FATAL_ERROR "runtime_deps_test: ${EXE} s PATH=$ENV{PATH} skoncil s '${_rc}' (ocekavano 0)\n${_out}${_err}")
endif()
message(STATUS "runtime_deps_test: ${EXE} OK bez MinGW v PATH")
