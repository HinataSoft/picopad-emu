// ****************************************************************************
//
//                 Host runtime - keys (placeholder — replaced in Task 5)
//
// ****************************************************************************
// Minimal link stub: every GPIO input reads 1 (buttons are active low, so all
// keys are released).

#include "host_internal.h"

u8 host_gpio_in(int pin)
{
	(void)pin;
	host_pump();
	return 1;
}
