#include "png.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned long crc_table[256];
static int crc_ready;

static void crc_init(void)
{
    unsigned long c;
    int n, k;
    if (crc_ready) return;
    for (n = 0; n < 256; n++) {
        c = (unsigned long)n;
        for (k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320ul ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    crc_ready = 1;
}

static unsigned long crc_update(unsigned long c, const unsigned char* p, size_t n)
{
    while (n--) c = crc_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return c;
}

static void be32(unsigned char* p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* Zapise chunk: delka, typ, data, CRC(typ+data). */
static int put_chunk(FILE* f, const char* type, const unsigned char* data, unsigned long n)
{
    unsigned char hdr[8], crc[4];
    unsigned long c;
    be32(hdr, n);
    hdr[4] = (unsigned char)type[0]; hdr[5] = (unsigned char)type[1];
    hdr[6] = (unsigned char)type[2]; hdr[7] = (unsigned char)type[3];
    c = crc_update(0xFFFFFFFFul, hdr + 4, 4);
    if (n) c = crc_update(c, data, n);
    be32(crc, c ^ 0xFFFFFFFFul);
    if (fwrite(hdr, 1, 8, f) != 8) return -1;
    if (n && fwrite(data, 1, n, f) != n) return -1;
    if (fwrite(crc, 1, 4, f) != 4) return -1;
    return 0;
}

int png_write_rgb565(const char* path, const unsigned short* fb, int w, int h, int stride)
{
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    unsigned char ihdr[13];
    unsigned char* raw;     /* nezabalena data: h * (1 + 3w) */
    unsigned char* idat;    /* zlib stream */
    size_t rawlen, nblocks, idatlen, pos, off, i;
    unsigned long a = 1, b = 0;
    FILE* f;
    int x, y, err = 0, e;

    if (!path || !fb || w <= 0 || h <= 0 || stride < w) {
        errno = EINVAL;
        return -1;
    }
    crc_init();

    rawlen = (size_t)h * (1 + 3 * (size_t)w);
    raw = (unsigned char*)malloc(rawlen);
    nblocks = (rawlen + 65534) / 65535;
    idatlen = 2 + rawlen + nblocks * 5 + 4;
    idat = (unsigned char*)malloc(idatlen);
    if (!raw || !idat) {
        free(raw);
        free(idat);
        errno = ENOMEM;
        return -1;
    }

    pos = 0;
    for (y = 0; y < h; y++) {
        const unsigned short* row = fb + (size_t)y * (size_t)stride;
        raw[pos++] = 0; /* filtr None */
        for (x = 0; x < w; x++) {
            unsigned p = row[x];
            unsigned r5 = (p >> 11) & 31, g6 = (p >> 5) & 63, b5 = p & 31;
            raw[pos++] = (unsigned char)((r5 << 3) | (r5 >> 2));
            raw[pos++] = (unsigned char)((g6 << 2) | (g6 >> 4));
            raw[pos++] = (unsigned char)((b5 << 3) | (b5 >> 2));
        }
    }

    /* zlib: hlavicka 0x78 0x01, stored bloky po max 65535 B, Adler-32 */
    pos = 0;
    idat[pos++] = 0x78;
    idat[pos++] = 0x01;
    for (off = 0; off < rawlen;) {
        size_t n = rawlen - off;
        if (n > 65535) n = 65535;
        idat[pos++] = (unsigned char)(off + n == rawlen ? 1 : 0);
        idat[pos++] = (unsigned char)(n & 0xFF);
        idat[pos++] = (unsigned char)(n >> 8);
        idat[pos++] = (unsigned char)(~n & 0xFF);
        idat[pos++] = (unsigned char)((~n >> 8) & 0xFF);
        for (i = 0; i < n; i++) {
            unsigned char v = raw[off + i];
            idat[pos++] = v;
            a = (a + v) % 65521;
            b = (b + a) % 65521;
        }
        off += n;
    }
    be32(idat + pos, (b << 16) | a);
    pos += 4;

    f = fopen(path, "wb");
    if (!f) {
        e = errno;
        free(raw);
        free(idat);
        errno = e;
        return -1;
    }
    be32(ihdr, (unsigned long)w);
    be32(ihdr + 4, (unsigned long)h);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* RGB */
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    if (fwrite(sig, 1, 8, f) != 8) err = 1;
    if (!err && put_chunk(f, "IHDR", ihdr, 13) != 0) err = 1;
    if (!err && put_chunk(f, "IDAT", idat, (unsigned long)pos) != 0) err = 1;
    if (!err && put_chunk(f, "IEND", NULL, 0) != 0) err = 1;
    e = errno;
    if (fclose(f) != 0 && !err) { err = 1; e = errno; }
    free(raw);
    free(idat);
    if (err) {
        errno = e;
        return -1;
    }
    return 0;
}
