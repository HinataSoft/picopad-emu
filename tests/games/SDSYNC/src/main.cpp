// ****************************************************************************
//
//     SDSYNC - headless test game for --sddir: reads /DATA/IN.TXT, writes
//              /DATA/OUT.TXT = "OK:" + the text, rewrites /DATA/MOD.TXT,
//              deletes /DATA/DEL.TXT, creates /NEWDIR/NEW.TXT, flushes,
//              shows DONE and then waits (key A ends the game itself)
//
// ****************************************************************************

#include "../include.h"

static void Show(const char* text, u16 color)
{
	DrawClear();
	DrawText2(text, 8, 100, color);
	DispUpdate();
}

// waits for ever; key A ends the game (ResetToBootLoader)
static void Wait()
{
	for (;;)
	{
		if (KeyGet() == KEY_A) ResetToBootLoader();
		WaitMs(10);
	}
}

// creates the file with the text and closes it
static Bool WriteFile(const char* path, const char* text, u32 n)
{
	sFile f;
	FileInit(&f);
	if (!FileCreate(&f, path)) return False;
	if (FileWrite(&f, text, n) != n) return False;
	return FileClose(&f);
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
		Wait();
	}

	FileInit(&f);
	if (!FileOpen(&f, "/DATA/IN.TXT"))
	{
		Show("NO IN.TXT", COL_RED);
		Wait();
	}
	n = FileRead(&f, text, sizeof(text) - 1);
	FileClose(&f);
	text[n] = 0;

	memcpy(line, "OK:", 3);
	memcpy(line + 3, text, n + 1);

	// FileCreate fails on an existing file: MOD.TXT is replaced by delete + create
	if (!WriteFile("/DATA/OUT.TXT", line, n + 3) ||
		!FileDelete("/DATA/MOD.TXT") ||
		!WriteFile("/DATA/MOD.TXT", "CHANGED", 7) ||
		!FileDelete("/DATA/DEL.TXT") ||
		!DirCreate("/NEWDIR") ||
		!WriteFile("/NEWDIR/NEW.TXT", "NEW", 3) ||
		!DiskFlush())
	{
		Show("WRITE ERR", COL_RED);
		Wait();
	}

	Show("DONE", COL_GREEN);
	Wait();
}
