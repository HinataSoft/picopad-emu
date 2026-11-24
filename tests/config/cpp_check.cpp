// C++ translation unit of sdk_objs_test: checks that the SDK headers
// (includes.h with host replacements) compile as C++ under MinGW g++,
// as all games are .cpp. Compile-only, the function is never called.
#include "include.h"

void cpp_check(void)
{
	DrawRect(10, 20, 30, 40, COL_WHITE);
	if (KeyGet() == KEY_A) PlaySound(NULL, 0);
	WaitMs((int)(Time() & 7));
	IRQ_LOCK;
	dmb();
	IRQ_UNLOCK;
}
