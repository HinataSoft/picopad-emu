// ****************************************************************************
//
//                  HELLO - example game outside of the SDK tree
//
// ****************************************************************************

#include "../include.h"

int main()
{
	// background and colored rectangles
	DrawClear();
	DrawRect(20, 20, 80, 60, COL_RED);
	DrawRect(120, 20, 80, 60, COL_GREEN);
	DrawRect(220, 20, 80, 60, COL_BLUE);
	DrawRect(70, 160, 180, 50, COL_YELLOW);

	// draw text
	DrawText2("HELLO PICOPAD", (WIDTH-13*8*2)/2, (HEIGHT-8*2)/2, COL_WHITE);
	DispUpdate();

	// reset to boot loader on key Y
	while (KeyGet() != KEY_Y) {}
	ResetToBootLoader();
}
