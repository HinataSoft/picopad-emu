// ****************************************************************************
//
//   CFGSAVE - headless test game: ConfigSave()/ConfigLoad() round trip
//
// ****************************************************************************
// Checks (in memory, no flash): the first ConfigLoad() gives the default
// volume, ConfigSave() followed by a volume change and ConfigLoad() restores
// the saved value. Success: ResetToBootLoader() (exit code 0). Mismatch: the
// game hangs, so the headless run ends with --max-time (exit code 3).

#include "../include.h"

#define SAVED_LEVEL	7	// ConfigSetVolume level, kept unchanged (7*10 = 70 <= 255)
#define OTHER_LEVEL	3

static void Fail()
{
	DrawClear();
	DrawText2("MISMATCH", 20, 20, COL_RED);
	DispUpdate();
	while (True) WaitMs(10);
}

int main()
{
	// before any save: defaults
	ConfigLoad();
	if (Config.volume != CONFIG_VOLUME_FULLSTEP) Fail();

	ConfigSetVolume(SAVED_LEVEL);
	if (Config.volume != SAVED_LEVEL*CONFIG_VOLUME_STEP) Fail();
	ConfigSave();

	ConfigSetVolume(OTHER_LEVEL);
	if (Config.volume != OTHER_LEVEL*CONFIG_VOLUME_STEP) Fail();
	ConfigLoad();
	if (Config.volume != SAVED_LEVEL*CONFIG_VOLUME_STEP) Fail();

	DrawClear();
	DrawText2("CONFIG OK", 20, 20, COL_GREEN);
	DispUpdate();
	WaitMs(1000);
	ResetToBootLoader();
}
