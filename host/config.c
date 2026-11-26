// ****************************************************************************
//
//                  Host runtime - device configuration (lib_config.c)
//
// ****************************************************************************
// The configuration lives only in memory: there is no persistent flash.
// ConfigSave() keeps a copy that ConfigLoad() restores later in the same run.
// cmake/picopad.cmake renames ConfigLoad, ConfigSave and ConfigClear inside
// lib_config.c (sdk_unused_*; the originals access flash), everything else of
// lib_config.c (Config, ConfigSetDef, ConfigSetVolume, ...) is the original.
// RuntimeInit() calls ConfigLoad() like the device runtime.

#include <string.h>

#include "host_internal.h"

#if USE_CONFIG

#include "_lib/inc/lib_config.h"

static sConfig Saved;		// copy made by ConfigSave() in this run
static int SavedValid;		// 1 = Saved holds a configuration

// load configuration: the copy saved earlier in this run (index 0), else the
// defaults (-1); nothing persists across runs
int ConfigLoad()
{
	if (SavedValid)
	{
		memcpy(&Config, &Saved, sizeof(sConfig));
		return 0;
	}
	ConfigSetDef(&Config);
	return -1;
}

// save configuration: in-memory copy only (no flash)
void ConfigSave()
{
	memcpy(&Saved, &Config, sizeof(sConfig));
	SavedValid = 1;
}

// clear stored configuration: discard the in-memory copy
void ConfigClear()
{
	SavedValid = 0;
}

#endif // USE_CONFIG
