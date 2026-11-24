// ****************************************************************************
//
//                      Host runtime - SD card (no card)
//
// ****************************************************************************
// lib_sd API (lib_sd.c is not compiled): the emulator has no SD card, so
// SDConnect() always fails and lib_fat sees no disk.

#include "host_internal.h"
#include "_lib/inc/lib_sd.h"

u8 SDType = SD_NONE;

const char* GetSDName() { return "none"; }
Bool SDConnect() { return False; }
void SDDisconnect() { }
Bool SDReadSect(u32 sector, u8* buffer) { (void)sector; (void)buffer; return False; }
Bool SDWriteSect(u32 sector, const u8* buffer) { (void)sector; (void)buffer; return False; }
u32 SDMediaSize() { return 0; }
void SDInit() { }
void SDTerm() { }
