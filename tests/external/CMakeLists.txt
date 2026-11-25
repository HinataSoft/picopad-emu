# Minimal external project (shape of the picopad-games repo): builds HELLO.exe
# from the emulator repo's examples using only PicopadEmu.cmake.
# Configure with -DPICOPAD_EMU_DIR=<picopad-emu> and the toolchain file.
cmake_minimum_required(VERSION 3.21)
project(mygames C CXX ASM)
include(${PICOPAD_EMU_DIR}/cmake/PicopadEmu.cmake)
picopad_add_game(HELLO ${PICOPAD_EMU_DIR}/examples/HELLO)
