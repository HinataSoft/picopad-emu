// ****************************************************************************
//
//    BEEP - headless test game: 1 kHz tone bit-banged on PWMSND_GPIO
//
// ****************************************************************************
// Like the TINYGAME games (USE_PWMSND 0): no PWM sound, the speaker pin is
// toggled with gpio_put and the half periods are timed with sleep_us.

#include "../include.h"

int main()
{
	int i;

	DrawClear();
	DrawText2("BEEP", (WIDTH-4*8*2)/2, (HEIGHT-8*2)/2, COL_WHITE);
	DispUpdate();

	// speaker pin as GPIO output (as PICOKIT of the TINYGAME games)
	gpio_init(PWMSND_GPIO);
	gpio_set_dir(PWMSND_GPIO, GPIO_OUT);

	// 500 ms of a 1 kHz square wave
	for (i = 0; i < 500; i++)
	{
		gpio_put(PWMSND_GPIO, true);
		sleep_us(500);
		gpio_put(PWMSND_GPIO, false);
		sleep_us(500);
	}

	ResetToBootLoader();
}
