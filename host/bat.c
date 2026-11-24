// ****************************************************************************
//
//                     Host runtime - battery (always full)
//
// ****************************************************************************
// picopad_bat API (picopad_bat.c needs the ADC): a full battery.

#include "host_internal.h"
#include "_devices/picopad/picopad_bat.h"

void BatInit() { }
void BatTerm() { }
float GetBat() { return 4.2f; }
int GetBatInt() { return 4200; }
