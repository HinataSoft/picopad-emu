// ****************************************************************************
//
//      HANG - headless test game: draws, then loops forever without SDK calls
//
// ****************************************************************************

#include "../include.h"

int main()
{
	DrawClear();
	DrawText2("HANG", (WIDTH-4*8*2)/2, (HEIGHT-8*2)/2, COL_WHITE);
	DispUpdate();

	// no SDK call ever again: only the watchdog advances time, and after an
	// end code only the platform's hang kill ends the process
	for (;;) {}
}
