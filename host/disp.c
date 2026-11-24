// ****************************************************************************
//
//                         Host runtime - display
//
// ****************************************************************************
// Replacement of the ST7789 driver. The SDK draws into FrameBuf and marks a
// dirty window; DispUpdate hands the window to the platform layer
// (plat_present) and models the SPI transfer time (24 MHz, 16 bits/px:
// w*h*16/24 us, full screen 51200 us) unless fast_display is set.
//
// FrameBuf, the dirty-window variables and DispDirtyAll/None/Rect/Point,
// DispAutoUpdate and DispUpdateAll are copied from _display/st7789/st7789.c:
// PicoLibSDK - Alternative SDK library for Raspberry Pico and RP2040
// Copyright (c) 2023 Miroslav Nemecek, Panda38@seznam.cz, hardyplotter2@gmail.com
//	https://github.com/Panda381/PicoLibSDK
// License: This source code is freely available for any purpose, including
// commercial. It is possible to take and modify the code or parts of it,
// without restriction.

#include "host_internal.h"
#include "_display/st7789/st7789.h"

// frame buffer in RGB 5-6-5 pixel format
ALIGNED u16 FrameBuf[FRAMESIZE];

// dirty window to update
int DispDirtyX1, DispDirtyX2, DispDirtyY1, DispDirtyY2;

// last system time of auto update
u32 DispAutoUpdateLast;

// set dirty all frame buffer
void DispDirtyAll()
{
	DispDirtyX1 = 0;
	DispDirtyX2 = WIDTH;
	DispDirtyY1 = 0;
	DispDirtyY2 = HEIGHT;
}

// set dirty none (clear after update)
void DispDirtyNone()
{
	DispDirtyX1 = WIDTH;
	DispDirtyX2 = 0;
	DispDirtyY1 = HEIGHT;
	DispDirtyY2 = 0;
}

// update dirty area by rectangle (check valid range)
void DispDirtyRect(int x, int y, int w, int h)
{
	if (x < 0)
	{
		w += x;
		x = 0;
	}
	if (x + w > WIDTH) w = WIDTH - x;
	if (w <= 0) return;

	if (y < 0)
	{
		h += y;
		y = 0;
	}
	if (y + h > HEIGHT) h = HEIGHT - y;
	if (h <= 0) return;

	if (x < DispDirtyX1) DispDirtyX1 = x;
	if (x + w > DispDirtyX2) DispDirtyX2 = x + w;
	if (y < DispDirtyY1) DispDirtyY1 = y;
	if (y + h > DispDirtyY2) DispDirtyY2 = y + h;
}

// update dirty area by pixel (check valid range)
void DispDirtyPoint(int x, int y)
{
	if (((u32)x < (u32)WIDTH) && ((u32)y < (u32)HEIGHT))
	{
		if (x < DispDirtyX1) DispDirtyX1 = x;
		if (x + 1 > DispDirtyX2) DispDirtyX2 = x + 1;
		if (y < DispDirtyY1) DispDirtyY1 = y;
		if (y + 1 > DispDirtyY2) DispDirtyY2 = y + 1;
	}
}

// update - send dirty window to display
void DispUpdate()
{
	int x = DispDirtyX1;
	int y = DispDirtyY1;
	int w = DispDirtyX2 - x;
	int h = DispDirtyY2 - y;

	// the window is kept inside the frame buffer by DispDirtyRect/Point; a
	// window set directly by a program is clamped here as well
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > WIDTH) w = WIDTH - x;
	if (y + h > HEIGHT) h = HEIGHT - y;

	if ((w > 0) && (h > 0))
	{
		plat_present(FrameBuf, x, y, w, h);

		// SPI transfer time: 16 bits per pixel at 24 MHz
		if (!host_cfg()->fast_display)
			host_wait_us((unsigned long long)w * (unsigned long long)h * 16 / 24);
	}

	DispDirtyNone();
	host_pump();
}

// auto update after delta time in [ms] of running program
void DispAutoUpdate(u32 ms)
{
	// interval in [us]
	u32 us = ms*1000;

	// check interval from last update
	if ((u32)(Time() - DispAutoUpdateLast) >= us)
	{
		// update display
		DispUpdate();

		// start measure new time interval of running program
		DispAutoUpdateLast = Time();
	}
}

// refresh update all display
void DispUpdateAll()
{
	// set dirty all frame buffer
	DispDirtyAll();

	// update - send dirty window to display
	DispUpdate();
}

void DispInit(u8 rot)
{
	(void)rot;
	DispDirtyNone();
}

void DispTerm()
{
}

// backlight is not emulated
void DispBacklight(u8 backlight)
{
	(void)backlight;
}

void DispBacklightUpdate()
{
}
