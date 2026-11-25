// ****************************************************************************
//
//   WDPANIC - headless test game: the mixer panics in a watchdog step
//
// ****************************************************************************
// After 'Armed' is set the game never calls the SDK again, so the next run of
// the PWM-wrap handler is a stall step of the host watchdog (another thread).

#include "../include.h"

extern "C" void PWMSndIrq();   // lib_pwmsnd.c (compiled as C), not in its header

static u8 Snd[4410];
static volatile int Armed;

static void Handler()
{
	if (Armed) panic("WDPANIC boom");
	PWMSndIrq();
}

int main()
{
	for (int i = 0; i < (int)sizeof(Snd); i++) Snd[i] = 0x80;

	DrawClear();
	DispUpdate();

	PlaySound(Snd, sizeof(Snd));
	SetHandler(IRQ_PWM_WRAP, Handler);
	Armed = 1;
	for (;;) {}
}
