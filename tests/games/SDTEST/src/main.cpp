// ****************************************************************************
//
//     SDTEST - headless test game: reads /TEST/IN.TXT from the SD card,
//              writes /TEST/OUT.TXT = "OK:" + the text, shows it and exits
//
// ****************************************************************************

#include "../include.h"

static void Show(const char* text, u16 color)
{
	DrawClear();
	DrawText2(text, 8, 100, color);
	DispUpdate();
}

static void Finish()
{
	WaitMs(2000);		// the script may end the run (quit, --max-time) before
	ResetToBootLoader();
}

int main()
{
	char text[64];
	char line[80];
	sFile f;
	u32 n;

	if (!DiskAutoMount())
	{
		Show("NO CARD", COL_RED);
		Finish();
	}

	FileInit(&f);
	if (!FileOpen(&f, "/TEST/IN.TXT"))
	{
		Show("NO IN.TXT", COL_RED);
		Finish();
	}
	n = FileRead(&f, text, sizeof(text) - 1);
	FileClose(&f);
	text[n] = 0;

	memcpy(line, "OK:", 3);
	memcpy(line + 3, text, n + 1);

	// write errors (read-only image) are shown, the run goes on
	FileInit(&f);
	if (!FileCreate(&f, "/TEST/OUT.TXT") ||
		FileWrite(&f, line, n + 3) != n + 3 ||
		!FileClose(&f) || !DiskFlush())
	{
		Show("WRITE ERR", COL_RED);
		Finish();
	}

	Show(line, COL_GREEN);
	Finish();
}
