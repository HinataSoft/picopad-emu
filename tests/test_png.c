/* Test zapisu PNG: signatura, IHDR, CRC IHDR; pixely overuje png_decode.py. */
#include "check.h"
#include "../platform/png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char* slurp(const char* path, long* size)
{
    FILE* f = fopen(path, "rb");
    unsigned char* b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (unsigned char*)malloc((size_t)*size);
    if (b && fread(b, 1, (size_t)*size, f) != (size_t)*size) { free(b); b = NULL; }
    fclose(f);
    return b;
}

static unsigned long be32(const unsigned char* p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) | p[3];
}

static unsigned long crc32_ref(const unsigned char* p, int n)
{
    unsigned long c = 0xFFFFFFFFul;
    int i, k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++) c = (c & 1) ? (c >> 1) ^ 0xEDB88320ul : c >> 1;
    }
    return c ^ 0xFFFFFFFFul;
}

static void check_png(const char* path, int w, int h)
{
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    long size = 0;
    unsigned char* b = slurp(path, &size);
    CHECK(b != NULL);
    if (!b) return;
    CHECK(size > 8 + 25 + 12);
    CHECK(memcmp(b, sig, 8) == 0);
    CHECK_EQ(be32(b + 8), 13);
    CHECK(memcmp(b + 12, "IHDR", 4) == 0);
    CHECK_EQ(be32(b + 16), w);
    CHECK_EQ(be32(b + 20), h);
    CHECK_EQ(b[24], 8);
    CHECK_EQ(b[25], 2);
    CHECK_EQ(b[26], 0);
    CHECK_EQ(b[27], 0);
    CHECK_EQ(b[28], 0);
    CHECK_EQ(be32(b + 29), crc32_ref(b + 12, 17));
    /* IEND na konci */
    CHECK(memcmp(b + size - 8, "IEND", 4) == 0);
    CHECK_EQ(be32(b + size - 4), crc32_ref(b + size - 8, 4));
    free(b);
}

int main(void)
{
    static unsigned short fb[320 * 240];
    unsigned short two[2] = {0xF800, 0x07E0};
    unsigned short padded[4] = {0xF800, 0x07E0, 0, 0};
    int x, y;

    for (y = 0; y < 240; y++)
        for (x = 0; x < 320; x++)
            fb[y * 320 + x] = (unsigned short)(((x * 31 / 319) << 11) |
                                               ((y * 63 / 239) << 5) | ((x + y) & 31));

    CHECK_EQ(png_write_rgb565("png_big.png", fb, 320, 240, 320), 0);
    CHECK_EQ(png_write_rgb565("png_two.png", two, 2, 1, 2), 0);
    CHECK_EQ(png_write_rgb565("png_stride.png", padded, 2, 1, 4), 0);
    CHECK_EQ(png_write_rgb565("no_such_dir/x.png", two, 2, 1, 2), -1);
    check_png("png_big.png", 320, 240);
    check_png("png_two.png", 2, 1);
    check_png("png_stride.png", 2, 1);
    return CHECK_DONE();
}
