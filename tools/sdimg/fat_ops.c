// ****************************************************************************
//
//          picopad-sdimg - FAT operations through PicoLibSDK lib_fat.c
//
// ****************************************************************************
// SDK side of the tool. lib_fat.c reaches the card only through the lib_sd API
// (SDConnect, SDReadSect, SDWriteSect, SDMediaSize, ...), which is implemented
// here on top of the image file functions img_* provided by main.c. No SDL,
// windows.h, pthread.h or <stdint.h> here; no host layer is linked, only the
// few SDK symbols lib_fat.c itself needs (see "stubs" below).

#include "include.h"

static const char* ErrMsg = "";

const char* fo_error(void) { return ErrMsg; }

static int fail(const char* msg) { ErrMsg = msg; return 1; }

// ---------------------------------------------------------------------------
//                      lib_sd API over the image file

u8 SDType = SD_NONE;

const char* GetSDName() { return "image"; }

Bool SDConnect()
{
	if (img_sectors() == 0) return False;
	SDType = SD_SDHC;
	return True;
}

void SDDisconnect() { SDType = SD_NONE; }

Bool SDReadSect(u32 sector, u8* buffer)
{
	return (sector < img_sectors()) && (img_read(sector, buffer) == 0);
}

Bool SDWriteSect(u32 sector, const u8* buffer)
{
	return (sector < img_sectors()) && (img_write(sector, buffer) == 0);
}

u32 SDMediaSize() { return img_sectors(); }

// ---------------------------------------------------------------------------
//        stubs of what lib_fat.c needs and the host layer would provide

// lib_fat.c is compiled with CheckApp/CheckAppPath/GetHomePath renamed (as for
// the games, see cmake/picopad.cmake); these are the replacements (no app in
// flash). Linked set: fat_ops.c, lib_fat.c, lib_stream.c.
Bool CheckApp(u32* applen, u32* proglen, u32* appcrc)
{
	(void)applen; (void)proglen; (void)appcrc;
	return False;
}

const sAppPath* CheckAppPath() { return NULL; }

int GetHomePath(char* path, const char* def)
{
	int n = (int)strlen(def);
	memcpy(path, def, n + 1);
	return n;
}

// Never called by the tool (FilePrint*, CheckLoaderData); defined here only
// so that lib_text.c, lib_print.c, lib_crc.c and their dependencies (DMA,
// drawing, memory pool, ...) need not be linked. StrLen is used by lib_fat.c
// itself (GetHomePath is renamed away, FilePrintText is unused).
int StrLen(const char* text) { return (int)strlen(text); }

u32 StreamPrintArg(sStream* wstr, sStream* rstr, va_list args)
{
	(void)wstr; (void)rstr; (void)args;
	return 0;
}

u32 Crc32ADMA(const void* buf, int len)
{
	(void)buf; (void)len;
	return 0;
}

// ---------------------------------------------------------------------------
//                              operations

int fo_mount(void)
{
	if (!DiskMount()) return fail("cannot mount the FAT file system of the image");
	return 0;
}

int fo_format(void)
{
	// DiskFormatDef() (= DiskFormat(FS_NONE, 0, True, ...)) cannot be used on
	// the host: with clust = 0 DiskFormat() starts with "mediasize / clust2" and
	// clust2 = 0. The RP2040 divider returns a harmless value there, but x86
	// raises a divide-by-zero fault. So the file system and cluster size that
	// the automatic mode would end up with are chosen here and passed
	// explicitly (MBR as in DiskFormatDef): FAT32 from 256 MB (cluster size 32
	// sectors, halved until DiskFormat accepts the cluster count), otherwise
	// FAT16 (8 sectors, halved), and FAT12 with 1-sector clusters for images
	// FAT16 cannot fill.
	u8 clust;
	if (img_sectors() >= 256u * 1024u * 2u)
	{
		for (clust = 32; clust > 0; clust >>= 1)
			if (DiskFormat(FS_FAT32, clust, True, FORMAT_MAGIC)) return fo_mount();
	}
	for (clust = 8; clust > 0; clust >>= 1)
		if (DiskFormat(FS_FAT16, clust, True, FORMAT_MAGIC)) return fo_mount();
	if (DiskFormat(FS_FAT12, 1, True, FORMAT_MAGIC)) return fo_mount();
	return fail("DiskFormat failed (image too small or I/O error)");
}

int fo_unmount(void)
{
	Bool ok = DiskFlush();
	DiskUnmount();
	if (!ok) return fail("flushing the image failed");
	return 0;
}

// lib_fat.c has no long file names: Disk_ExtractName() silently cuts a name to
// 8 characters and an extension to 3 and accepts characters that are not valid
// in a short name. fo_check_name() refuses all of that, so a file is never
// stored under a different name than asked for.
static int valid_char(unsigned char c)
{
	if (c >= 'A' && c <= 'Z') return 1;
	if (c >= 'a' && c <= 'z') return 1;
	if (c >= '0' && c <= '9') return 1;
	return c != 0 && strchr("!#$%&'()-@^_`{}~", c) != NULL;
}

int fo_check_name(const char* path)
{
	const char* p = path;
	for (;;)
	{
		while (*p == '/' || *p == '\\') p++;
		if (*p == 0) return 0;
		int base = 0, ext = 0, dot = 0;
		for (; *p != 0 && *p != '/' && *p != '\\'; p++)
		{
			if (*p == '.')
			{
				if (dot || base == 0) return fail("not a valid 8.3 name (lib_fat has no long file names)");
				dot = 1;
			}
			else if (!valid_char((unsigned char)*p) || (dot ? ++ext > 3 : ++base > 8))
				return fail("not a valid 8.3 name (lib_fat has no long file names)");
		}
		if (dot && ext == 0) return fail("not a valid 8.3 name (lib_fat has no long file names)");
	}
}

int fo_mkdir(const char* path)
{
	if (fo_check_name(path)) return 1;
	u8 attr = GetFileAttr(path);
	if (attr != ATTR_NONE)
	{
		if (attr & ATTR_DIR) return 0;
		return fail("a file with the name of the directory already exists");
	}
	if (!DirCreate(path)) return fail("DirCreate failed (disk full or root directory full)");
	return 0;
}

static sFile WFile, RFile;
static sFile FindFile;

int fo_write_begin(const char* path)
{
	if (fo_check_name(path)) return 1;
	if (GetFileAttr(path) != ATTR_NONE)
	{
		if (!FileDelete(path)) return fail("cannot replace the existing file");
	}
	FileInit(&WFile);
	if (!FileCreate(&WFile, path)) return fail("FileCreate failed (disk full or directory missing)");
	return 0;
}

int fo_write(const void* data, unsigned size)
{
	if (FileWrite(&WFile, data, size) != size) return fail("write failed (disk full?)");
	return 0;
}

int fo_write_end(void)
{
	if (!FileClose(&WFile)) return fail("closing the file failed");
	return 0;
}

int fo_read_begin(const char* path, unsigned* size)
{
	if (fo_check_name(path)) return 1;
	FileInit(&RFile);
	if (!FileOpen(&RFile, path)) return fail("file not found");
	if (RFile.attr & ATTR_DIR) { FileClose(&RFile); return fail("is a directory"); }
	*size = FileSize(&RFile);
	return 0;
}

int fo_read(void* data, unsigned size, unsigned* got)
{
	*got = FileRead(&RFile, data, size);
	return 0;
}

int fo_read_end(void)
{
	FileClose(&RFile);
	return 0;
}

int fo_list(const char* path, fo_list_cb cb, void* ctx)
{
	sFileInfo fi;
	if (fo_check_name(path)) return 1;
	FileInit(&FindFile);
	if (!FindOpen(&FindFile, path)) return fail("directory not found");
	while (FindNext(&FindFile, &fi, ATTR_DIR_MASK, "*"))
	{
		if (fi.name[0] == '.') continue;
		cb(ctx, (fi.attr & ATTR_DIR) != 0, fi.name, fi.size);
	}
	FindClose(&FindFile);
	return 0;
}
