// ****************************************************************************
//
//                 Host runtime - keys (GPIO shim for KeyScan)
//
// ****************************************************************************
// The SDK's KeyScan() reads the buttons with GPIO_In(pin) (active low). The
// platform layer supplies the pressed keys as a bitmask (plat_key_bits).

#include "host_internal.h"
#include "../include/plat_api.h"
#include "_devices/picopad/picopad_key.h"

u8 host_gpio_in(int pin)
{
	int code = 0;
	host_pump();
	switch (pin) {
	case BTN_UP_PIN:    code = KEY_UP;    break;
	case BTN_LEFT_PIN:  code = KEY_LEFT;  break;
	case BTN_RIGHT_PIN: code = KEY_RIGHT; break;
	case BTN_DOWN_PIN:  code = KEY_DOWN;  break;
	case BTN_X_PIN:     code = KEY_X;     break;
	case BTN_Y_PIN:     code = KEY_Y;     break;
	case BTN_A_PIN:     code = KEY_A;     break;
	case BTN_B_PIN:     code = KEY_B;     break;
	default: return 1;
	}
	return (plat_key_bits() & PLAT_KEY_BIT(code)) ? 0 : 1;
}
