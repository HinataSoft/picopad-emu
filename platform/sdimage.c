/* SD card image file, see sdimage.h. */
#include "sdimage.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static FILE* Img;
static unsigned int Sectors;
static int ReadOnly;

static int fail(char* err, int errlen, const char* fmt, ...)
{
    if (err && errlen > 0)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(err, (size_t)errlen, fmt, args);
        va_end(args);
    }
    return -1;
}

void sdimage_close(void)
{
    if (Img) fclose(Img);
    Img = NULL;
    Sectors = 0;
    ReadOnly = 0;
}

int sdimage_open(const char* path, char* err, int errlen)
{
    struct _stati64 st;
    FILE* f;
    int ro = 0;
    unsigned long long size;

    sdimage_close();
    if (err && errlen > 0) err[0] = 0;

    if (_stati64(path, &st) != 0)
        return fail(err, errlen, "cannot open SD image '%s': %s", path, strerror(errno));
    if (st.st_mode & _S_IFDIR)
        return fail(err, errlen, "SD image '%s' is a directory", path);
    size = (unsigned long long)st.st_size;
    if (size % SDIMAGE_SECTOR != 0)
        return fail(err, errlen, "SD image '%s': size %lld bytes is not a multiple of 512", path, (long long)size);
    if (size < SDIMAGE_MIN_BYTES)
        return fail(err, errlen, "SD image '%s': size %lld bytes is below the minimum of 64 KiB (65536 bytes)",
                    path, (long long)size);
    if (size / SDIMAGE_SECTOR > 0xFFFFFFF0ull)
        return fail(err, errlen, "SD image '%s': size %lld bytes is too large (at most 2 TiB)", path, (long long)size);

    f = fopen(path, "r+b");
    if (!f)
    {
        f = fopen(path, "rb");   /* read-only file: usable, writes fail */
        ro = 1;
    }
    if (!f)
        return fail(err, errlen, "cannot open SD image '%s': %s", path, strerror(errno));
    Img = f;
    ReadOnly = ro;
    Sectors = (unsigned int)(size / SDIMAGE_SECTOR);
    return 0;
}

unsigned int sdimage_sectors(void)
{
    return Img ? Sectors : 0;
}

int sdimage_read(unsigned int sector, unsigned char* buf512)
{
    if (!Img || sector >= Sectors) return 0;
    if (_fseeki64(Img, (long long)sector * SDIMAGE_SECTOR, SEEK_SET) != 0) return 0;
    return fread(buf512, 1, SDIMAGE_SECTOR, Img) == SDIMAGE_SECTOR;
}

int sdimage_write(unsigned int sector, const unsigned char* buf512)
{
    if (!Img || ReadOnly || sector >= Sectors) return 0;
    if (_fseeki64(Img, (long long)sector * SDIMAGE_SECTOR, SEEK_SET) != 0) return 0;
    if (fwrite(buf512, 1, SDIMAGE_SECTOR, Img) != SDIMAGE_SECTOR) return 0;
    return fflush(Img) == 0;   /* in the OS at once: survives any end of the process */
}
