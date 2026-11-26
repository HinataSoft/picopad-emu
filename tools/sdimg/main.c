/* picopad-sdimg: formats and reads/writes FAT SD card images through the SDK's
 * lib_fat.c (see fat_ops.c), so an image is exactly what the device reads.
 *
 *   picopad-sdimg create IMG [--size MB] [--from DIR]
 *   picopad-sdimg ls IMG [PATH]
 *   picopad-sdimg get IMG PATH OUT
 *   picopad-sdimg put IMG PATH IN
 *
 * Exit codes: 0 ok, 1 operation error, 2 bad arguments.
 * Pure C: no SDK headers (the SDK side is fat_ops.c, interface fat_ops.h). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "fat_ops.h"

#define SECT 512u
#define DEF_MB 64u
#define MAX_MB 4096u
#define MIN_BYTES (64u * 1024u)

static FILE* Img;
static unsigned ImgSectors;

unsigned img_sectors(void) { return ImgSectors; }

static int seek_sector(unsigned sector)
{
	return _fseeki64(Img, (__int64)sector * SECT, SEEK_SET);
}

int img_read(unsigned sector, unsigned char* buf)
{
	if (seek_sector(sector) != 0) return 1;
	return fread(buf, 1, SECT, Img) == SECT ? 0 : 1;
}

int img_write(unsigned sector, const unsigned char* buf)
{
	if (seek_sector(sector) != 0) return 1;
	return fwrite(buf, 1, SECT, Img) == SECT ? 0 : 1;
}

static int usage(const char* msg)
{
	if (msg) fprintf(stderr, "picopad-sdimg: %s\n", msg);
	fprintf(stderr,
		"usage: picopad-sdimg create IMG [--size MB] [--from DIR]\n"
		"       picopad-sdimg ls IMG [PATH]\n"
		"       picopad-sdimg get IMG PATH OUT\n"
		"       picopad-sdimg put IMG PATH IN\n");
	return 2;
}

static int op_error(const char* what, const char* name)
{
	fprintf(stderr, "picopad-sdimg: %s: %s%s%s\n", what, name ? name : "",
		name ? ": " : "", fo_error());
	return 1;
}

static int msg_error(const char* what, const char* name)
{
	fprintf(stderr, "picopad-sdimg: %s %s\n", what, name);
	return 1;
}

/* open an existing image for ls/get (read only) or put (read/write) */
static int open_image(const char* path, int write)
{
	__int64 size;
	Img = fopen(path, write ? "r+b" : "rb");
	if (!Img) return msg_error("cannot open image", path);
	_fseeki64(Img, 0, SEEK_END);
	size = _ftelli64(Img);
	if (size < (__int64)MIN_BYTES || size % SECT != 0 || size > (__int64)MAX_MB * 1024 * 1024)
	{
		fclose(Img);
		fprintf(stderr, "picopad-sdimg: %s: not a valid image (size must be a multiple of 512, 64 KiB..4 GiB)\n", path);
		return 1;
	}
	ImgSectors = (unsigned)(size / SECT);
	return 0;
}

/* ---- copy a host file into / out of the image ---- */

static int put_file(const char* imgpath, const char* hostpath)
{
	static unsigned char buf[64 * 1024];
	size_t n;
	int rc = 0;
	FILE* f = fopen(hostpath, "rb");
	if (!f) return msg_error("cannot read", hostpath);
	if (fo_write_begin(imgpath)) { fclose(f); return op_error("cannot create", hostpath); }
	while ((n = fread(buf, 1, sizeof buf, f)) > 0)
	{
		if (fo_write(buf, (unsigned)n)) { rc = op_error("cannot write", hostpath); break; }
	}
	if (rc == 0 && ferror(f)) rc = msg_error("read error", hostpath);
	if (fo_write_end() && rc == 0) rc = op_error("cannot close", hostpath);
	fclose(f);
	return rc;
}

static int get_file(const char* imgpath, const char* hostpath)
{
	static unsigned char buf[64 * 1024];
	unsigned size, got, left;
	FILE* f;
	int rc = 0;
	if (fo_read_begin(imgpath, &size)) return op_error("cannot read", imgpath);
	f = fopen(hostpath, "wb");
	if (!f) { fo_read_end(); return msg_error("cannot write", hostpath); }
	for (left = size; left > 0;)
	{
		unsigned want = left < sizeof buf ? left : (unsigned)sizeof buf;
		fo_read(buf, want, &got);
		if (got != want) { rc = msg_error("read error in the image:", imgpath); break; }
		if (fwrite(buf, 1, got, f) != got) { rc = msg_error("write error", hostpath); break; }
		left -= got;
	}
	fo_read_end();
	if (fclose(f) != 0 && rc == 0) rc = msg_error("write error", hostpath);
	return rc;
}

/* ---- copy a host directory tree into the image ---- */

static int copy_tree(const char* hostdir, const char* imgdir)
{
	char pattern[MAX_PATH + 8];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	int rc = 0;
	if (strlen(hostdir) + 3 >= sizeof pattern) return msg_error("path too long:", hostdir);
	sprintf(pattern, "%s\\*", hostdir);
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) return msg_error("cannot read directory", hostdir);
	do
	{
		char host[MAX_PATH + 8], img[1024];
		if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
		if (strlen(hostdir) + strlen(fd.cFileName) + 2 >= sizeof host ||
			strlen(imgdir) + strlen(fd.cFileName) + 2 >= sizeof img)
		{ rc = msg_error("path too long:", fd.cFileName); break; }
		sprintf(host, "%s\\%s", hostdir, fd.cFileName);
		sprintf(img, "%s/%s", imgdir, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (fo_mkdir(img)) { rc = op_error("cannot create directory", host); break; }
			rc = copy_tree(host, img);
		}
		else
			rc = put_file(img, host);
		if (rc) break;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
	return rc;
}

/* ---- commands ---- */

static int parse_mb(const char* s, unsigned* mb)
{
	unsigned long v = 0;
	if (!*s) return 0;
	for (; *s; s++)
	{
		if (*s < '0' || *s > '9') return 0;
		v = v * 10 + (unsigned long)(*s - '0');
		if (v > MAX_MB) return 0;
	}
	if (v < 1) return 0;
	*mb = (unsigned)v;
	return 1;
}

static int cmd_create(int argc, char** argv)
{
	const char* img = argv[0];
	const char* from = NULL;
	unsigned mb = DEF_MB;
	int i, rc;
	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "--size"))
		{
			if (++i >= argc || !parse_mb(argv[i], &mb)) return usage("--size needs a number of MB, 1..4096");
		}
		else if (!strcmp(argv[i], "--from"))
		{
			if (++i >= argc) return usage("--from needs a directory");
			from = argv[i];
		}
		else return usage("unknown option for create");
	}
	if (from)
	{
		DWORD a = GetFileAttributesA(from);
		if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY))
			return msg_error("--from: not a directory:", from);
	}
	Img = fopen(img, "w+b");
	if (!Img) return msg_error("cannot create", img);
	ImgSectors = mb * (1024u * 1024u / SECT);
	if (_fseeki64(Img, (__int64)ImgSectors * SECT - 1, SEEK_SET) != 0 || fputc(0, Img) == EOF)
	{
		fclose(Img); remove(img);
		return msg_error("cannot allocate the image", img);
	}
	rc = fo_format();
	if (rc) rc = op_error("format failed", img);
	else if (from) rc = copy_tree(from, "");
	if (fo_unmount() && rc == 0) rc = op_error("cannot finish", img);
	if (fclose(Img) != 0 && rc == 0) rc = msg_error("write error", img);
	if (rc) remove(img);
	return rc;
}

struct args { const char *path, *file; };

static void ls_cb(void* ctx, int is_dir, const char* name, unsigned size)
{
	(void)ctx;
	if (is_dir) printf("D %s\n", name);
	else printf("F %s %u\n", name, size);
}

static int do_ls(struct args* a)
{
	if (fo_list(a->path, ls_cb, NULL)) return op_error("ls", a->path);
	return 0;
}
static int do_get(struct args* a) { return get_file(a->path, a->file); }
static int do_put(struct args* a) { return put_file(a->path, a->file); }

static int with_image(const char* img, int write, int (*fn)(struct args*), struct args* a)
{
	int rc = open_image(img, write);
	if (rc) return rc;
	if (fo_mount()) { rc = op_error(img, NULL); fclose(Img); return rc; }
	rc = fn(a);
	if (fo_unmount() && rc == 0) rc = op_error("cannot finish", img);
	fclose(Img);
	return rc;
}

int main(int argc, char** argv)
{
	struct args a;
	if (argc < 2) return usage("missing command");
	if (!strcmp(argv[1], "create"))
	{
		if (argc < 3) return usage("create needs an image name");
		return cmd_create(argc - 2, argv + 2);
	}
	if (!strcmp(argv[1], "ls"))
	{
		if (argc < 3 || argc > 4) return usage("ls IMG [PATH]");
		a.path = argc == 4 ? argv[3] : "/"; a.file = NULL;
		return with_image(argv[2], 0, do_ls, &a);
	}
	if (!strcmp(argv[1], "get"))
	{
		if (argc != 5) return usage("get IMG PATH OUT");
		a.path = argv[3]; a.file = argv[4];
		return with_image(argv[2], 0, do_get, &a);
	}
	if (!strcmp(argv[1], "put"))
	{
		if (argc != 5) return usage("put IMG PATH IN");
		a.path = argv[3]; a.file = argv[4];
		return with_image(argv[2], 1, do_put, &a);
	}
	return usage("unknown command");
}
