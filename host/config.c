// ****************************************************************************
//
//                  Host runtime - device configuration (lib_config.c)
//
// ****************************************************************************
// The configuration lives only in memory: there is no persistent flash.
// cmake/picopad.cmake renames ConfigLoad, ConfigSave and ConfigClear inside
// lib_config.c (sdk_unused_*; the originals access flash), everything else of
// lib_config.c (Config, ConfigSetDef, ConfigSetVolume, ...) is the original.
// RuntimeInit() calls ConfigLoad() like the device runtime.

#include "host_internal.h"

#if USE_CONFIG

#include "_lib/inc/lib_config.h"

// load configuration: always the defaults (nothing is stored)
int ConfigLoad()
{
	ConfigSetDef(&Config);
	return -1;
}

// save configuration: it stays in memory until the end of the run
void ConfigSave()
{
}

// clear stored configuration: nothing is stored
void ConfigClear()
{
}

#endif // USE_CONFIG
