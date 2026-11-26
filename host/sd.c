// ****************************************************************************
//
//                      Host runtime - SD card (image file)
//
// ****************************************************************************
// lib_sd API (lib_sd.c is not compiled) over the plat_sd_* sector I/O of the
// platform (an SD card image given by --sd; without it there is no card, so
// SDConnect() fails and lib_fat sees no disk). The card is always SDHC.

#include "host_internal.h"
#include "_lib/inc/lib_sd.h"

u8 SDType = SD_NONE;

const char* GetSDName() { return SDType == SD_NONE ? "none" : "SDHC"; }

Bool SDConnect()
{
	if (plat_sd_sectors() == 0)
	{
		SDType = SD_NONE;
		return False;
	}
	SDType = SD_SDHC;
	return True;
}

void SDDisconnect() { SDType = SD_NONE; }

Bool SDReadSect(u32 sector, u8* buffer)
{
	return plat_sd_read(sector, buffer) ? True : False;
}

Bool SDWriteSect(u32 sector, const u8* buffer)
{
	return plat_sd_write(sector, buffer) ? True : False;
}

u32 SDMediaSize() { return plat_sd_sectors(); }

void SDInit() { }
void SDTerm() { }
