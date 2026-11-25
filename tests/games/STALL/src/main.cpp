// ****************************************************************************
//
//     STALL - headless test game: waits for a sound without any SDK call
//
// ****************************************************************************

#include "../include.h"

// 0.2 s of a square wave at 22050 Hz (8-bit unsigned samples)
static u8 Snd[4410];

int main()
{
	for (int i = 0; i < (int)sizeof(Snd); i++) Snd[i] = ((i / 25) & 1) ? 0xC0 : 0x40;

	DrawClear();
	DispUpdate();

	PlaySound(Snd, sizeof(Snd));
	while (PlayingSound()) {}	// no SDK call that pumps: only the watchdog unsticks it

	DrawRect(70, 160, 180, 50, COL_GREEN);
	DispUpdate();
	ResetToBootLoader();
}
