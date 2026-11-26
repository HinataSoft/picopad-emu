// ****************************************************************************
//
//   CFGSAVE - headless test game: ConfigSave() must not crash (no flash)
//
// ****************************************************************************
// ConfigSetVolume(50); ConfigSave(); ConfigLoad(); then Config.volume is shown
// as text on the display, one second wait, ResetToBootLoader().

#include "../include.h"

int main()
{
	char buf[16];
	int i = 0, n, d;

	ConfigSetVolume(50);
	ConfigSave();
	ConfigLoad();

	// Config.volume as decimal text (no printf dependency)
	n = Config.volume;
	for (d = 100; d > 0; d /= 10)
		if ((n >= d) || (i > 0) || (d == 1)) { buf[i++] = (char)('0' + (n / d) % 10); }
	buf[i] = 0;

	DrawClear();
	DrawText2("VOLUME", 20, 20, COL_WHITE);
	DrawText2(buf, 20, 60, COL_YELLOW);
	DispUpdate();
	WaitMs(1000);
	ResetToBootLoader();
}
