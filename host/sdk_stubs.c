// ****************************************************************************
//
//          Host runtime - replacements of SDK parts that need hardware
//
// ****************************************************************************
// Definitions of SDK symbols whose original source is not compiled for the
// host (sdk_bootrom.c, sdk_clocks.c, sdk_dma.c, USB host stack) and host
// replacements of the lib_fat.c functions that read the flash.
//
// lib_fat.c: CheckApp() reads the application header directly from flash at
// XIP_BASE + BOOTLOADER_SIZE, which is not mapped on the host. That cannot be
// avoided without changing the SDK, so cmake/picopad.cmake renames CheckApp,
// CheckAppPath and GetHomePath inside lib_fat.c only (sdk_unused_*; they call
// each other) and the versions below report "no application in flash".
// CheckLoaderData() stays original: it reads LoaderData[] (RAM, defined in
// lib_fat.c, zeros = invalid loader data, returns False).

#include <string.h>

#include "host_internal.h"
#include "_sdk/inc/sdk_clocks.h"		// CurrentFreq
#include "_sdk/usb_inc/sdk_usb_phy.h"		// UsbHostIsInit, UsbTerm
#include "_sdk/usb_inc/sdk_usb_host.h"		// UsbHostInit
#include "_sdk/usb_inc/sdk_usb_host_hid.h"	// UsbKey*, UsbGetChar, UsbFlushKey
#include "_lib/inc/lib_text.h"			// StrLen
#include "_lib/inc/lib_fat.h"			// CheckApp, sAppPath, SetDir
#include "_sdk/inc/sdk_flash.h"			// FLASH_ID_SIZE, FlashErase, FlashProgram
#include "_sdk/inc/sdk_adc.h"			// ADC_Temp
#include "_sdk/usb_inc/sdk_usbport.h"		// UsbPort* (USE_USBPORT)

// ---------------------------------------------------------------------------
// ROM function pointers (sdk_bootrom.c, RP2040)

static u32 HostPopcount(u32 val) { return (u32)__builtin_popcount(val); }
static u32 HostReverse(u32 val) { return Reverse32(val); }
static u32 HostClz(u32 val) { return Clz(val); }	// 0 -> 32 as the ROM
static u32 HostCtz(u32 val) { return Ctz(val); }	// 0 -> 32 as the ROM
static void* HostMemSet(void* dst, u8 val, u32 size) { return memset(dst, val, size); }
static u32* HostMemSet32(u32* dst, u8 val, u32 size) { return (u32*)memset(dst, val, size); }
static void* HostMemCopy(void* dst, const void* src, u32 size) { return memcpy(dst, src, size); }
static u32* HostMemCopy32(u32* dst, const u32* src, u32 size) { return (u32*)memcpy(dst, src, size); }

rom_popcount32_fn	popcount = HostPopcount;
rom_reverse32_fn	reverse = HostReverse;
rom_clz32_fn		clz = HostClz;
rom_ctz32_fn		ctz = HostCtz;
rom_memset_fn		MemSet = HostMemSet;
rom_memset4_fn		MemSet32 = HostMemSet32;
rom_memcpy_fn		MemCopy = HostMemCopy;
rom_memcpy44_fn		MemCopy32 = HostMemCopy32;

// (re)initialize ROM function pointers (called from RuntimeInit)
void RomFncInit(void)
{
	popcount = HostPopcount;
	reverse = HostReverse;
	clz = HostClz;
	ctz = HostCtz;
	MemSet = HostMemSet;
	MemSet32 = HostMemSet32;
	MemCopy = HostMemCopy;
	MemCopy32 = HostMemCopy32;
}

// ---------------------------------------------------------------------------
// clocks (sdk_clocks.c); CLK_SYS is set by RuntimeInit

u32 CurrentFreq[CLK_SRC_NUM];

// ---------------------------------------------------------------------------
// DMA sniffer checksums (sdk_dma.c), computed in software.
// The data are fed as a byte stream in memory order (the SDK uses DMA_CRC as
// an equivalent of its software CRCs, for any alignment).

static u32 SniffBytes(int mode, u32 s, const u8* d, int n)
{
	int i;
	for (; n > 0; n--)
	{
		u8 b = *d++;
		switch (mode)
		{
		case DMA_CRC_CRC32REV:
			b = Reverse8(b);
			// fall through
		case DMA_CRC_CRC32:
			s ^= (u32)b << 24;
			for (i = 0; i < 8; i++) s = (s & 0x80000000UL) ? ((s << 1) ^ 0x04C11DB7UL) : (s << 1);
			break;

		case DMA_CRC_CRC16REV:
			b = Reverse8(b);
			// fall through
		case DMA_CRC_CRC16:
			s ^= (u32)b << 8;
			for (i = 0; i < 8; i++) s = (s & 0x8000) ? ((s << 1) ^ 0x1021) : (s << 1);
			s &= 0xffff;
			break;

		case DMA_CRC_XOR:
			s ^= (u32)__builtin_parity(b);
			break;
		}
	}
	return s;
}

// feed 'num' transfers of 'size' (DMA_SIZE_8/16/32) bytes each
static u32 SniffFeed(int mode, u32 s, const u8* d, int num, int size)
{
	int bytes = 1 << size;
	if ((mode & DMA_CRC_MASK) == DMA_CRC_SUM)
	{
		for (; num > 0; num--, d += bytes)
		{
			u32 v = d[0];
			if (bytes >= 2) v |= (u32)d[1] << 8;
			if (bytes == 4) v |= ((u32)d[2] << 16) | ((u32)d[3] << 24);
			s += v;
		}
		return s;
	}
	return SniffBytes(mode & DMA_CRC_MASK, s, d, num * bytes);
}

static u32 SniffResult(int mode, u32 s)
{
	if ((mode & DMA_CRC_REV) != 0) s = Reverse32(s);
	if ((mode & DMA_CRC_INV) != 0) s = ~s;
	return s;
}

u32 DMA_CRC(int mode, u32 init, int dma, const void* data, int num)
{
	(void)dma;
	const u8* d = (const u8*)data;
	u32 s = init;
	int n;

	// same split as the original: leading bytes, aligned words, tail bytes
	if ((((u32)d & 3) != 0) && (num > 0))
	{
		n = 4 - ((u32)d & 3);
		if (n > num) n = num;
		s = SniffFeed(mode, s, d, n, DMA_SIZE_8);
		d += n;
		num -= n;
	}
	n = num >> 2;
	if (n > 0)
	{
		s = SniffFeed(mode, s, d, n, DMA_SIZE_32);
		d += n*4;
		num -= n*4;
	}
	if (num > 0) s = SniffFeed(mode, s, d, num, DMA_SIZE_8);

	return SniffResult(mode, s);
}

u32 DMA_SUM(int mode, u32 init, int dma, const void* data, int num, int size)
{
	(void)dma;
	return SniffResult(mode, SniffFeed(mode, init, (const u8*)data, num, size));
}

// ---------------------------------------------------------------------------
// USB host (USE_USBPAD): no USB device is ever connected

Bool UsbHostIsInit = False;

void UsbHostInit() { host_pump(); }
void UsbTerm() { host_pump(); }
Bool UsbKeyIsMounted() { host_pump(); return False; }
Bool UsbKeyIsPressed(u8 key) { (void)key; host_pump(); return False; }
u32 UsbGetKeyRel() { host_pump(); return 0; }
char UsbGetChar() { host_pump(); return 0; }
void UsbFlushKey() { host_pump(); }

// ---------------------------------------------------------------------------
// lib_fat.c: no application in flash (see top of file)

Bool CheckApp(u32* applen, u32* proglen, u32* appcrc)
{
	(void)applen; (void)proglen; (void)appcrc;
	return False;
}

const sAppPath* CheckAppPath()
{
	return NULL;
}

// GetHomePath of lib_fat.c without the flash path: always the default
// (PicoLibSDK, Copyright (c) 2023 Miroslav Nemecek, freely available)
int GetHomePath(char* path, const char* def)
{
	int len = StrLen(def);
	memcpy(path, def, len+1);
	SetDir(path);
	return len;
}

// ---------------------------------------------------------------------------
// flash (sdk_flash.c): info of a 2 MB chip; the game must not program it

char FlashIDText[FLASH_ID_SIZE*2+1] = "E660000000000000";
u32 FlashSize = 2*1024*1024;

void FlashErase(u32 addr, u32 count)
{
	(void)addr; (void)count;
	host_fatal("FlashErase: direct flash access is not supported by the emulator");
}

void FlashProgram(u32 addr, const u8* data, u32 count)
{
	(void)addr; (void)data; (void)count;
	host_fatal("FlashProgram: direct flash access is not supported by the emulator");
}

// ---------------------------------------------------------------------------
// ADC temperature sensor (sdk_adc.c): fixed 27 degrees Celsius

float ADC_Temp(void)
{
	return 27.0f;
}

// ---------------------------------------------------------------------------
// USB Mini-Port (USE_USBPORT, sdk_usbport.c): the opposite device is never
// connected; sent packets are discarded, nothing is received

#if USE_USBPORT

u8 UsbPort = USBPORT_OFF;

void UsbPortInit(u8 port) { UsbPort = port; host_pump(); }
void UsbPortTerm() { UsbPort = USBPORT_OFF; host_pump(); }
Bool UsbPortConnected() { host_pump(); return False; }
Bool UsbPortSendReady() { host_pump(); return True; }
Bool UsbPortSend(const void* pkt) { (void)pkt; host_pump(); return True; }
Bool UsbPortRecv(void* pkt) { (void)pkt; host_pump(); return False; }

#endif // USE_USBPORT
