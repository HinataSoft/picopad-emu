// ****************************************************************************
//
//          KEYWAIT - headless test game: waits for a key, then exits
//
// ****************************************************************************

#include "../include.h"

int main()
{
	DrawClear();
	DrawText2("PRESS A", (WIDTH-7*8*2)/2, (HEIGHT-8*2)/2, COL_WHITE);
	DispUpdate();

	// wait for any key (the script presses A)
	while (KeyGet() == NOKEY) {}

	DrawRect(70, 160, 180, 50, COL_GREEN);
	DispUpdate();
	WaitMs(100);
	ResetToBootLoader();
}
