#include "wav.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct wav_writer {
    FILE* f;
    unsigned long bytes; /* velikost dat */
    int err;
};

static void put32(unsigned char* p, unsigned long v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void put16(unsigned char* p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void make_header(unsigned char* h, int rate)
{
    h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
    put32(h + 4, 36);
    h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
    h[12] = 'f'; h[13] = 'm'; h[14] = 't'; h[15] = ' ';
    put32(h + 16, 16);
    put16(h + 20, 1);
    put16(h + 22, 1);
    put32(h + 24, (unsigned long)rate);
    put32(h + 28, (unsigned long)rate * 2);
    put16(h + 32, 2);
    put16(h + 34, 16);
    h[36] = 'd'; h[37] = 'a'; h[38] = 't'; h[39] = 'a';
    put32(h + 40, 0);
}

wav_writer* wav_open(const char* path, int rate)
{
    unsigned char h[44];
    wav_writer* w;
    FILE* f = fopen(path, "wb");
    if (!f) return NULL;
    make_header(h, rate);
    if (fwrite(h, 1, 44, f) != 44) {
        int e = errno;
        fclose(f);
        errno = e;
        return NULL;
    }
    w = (wav_writer*)calloc(1, sizeof *w);
    if (!w) {
        fclose(f);
        errno = ENOMEM;
        return NULL;
    }
    w->f = f;
    return w;
}

int wav_write(wav_writer* w, const short* s, int n)
{
    unsigned char buf[2048];
    int i;
    if (!w || w->err) return -1;
    while (n > 0) {
        int k = n > 1024 ? 1024 : n;
        for (i = 0; i < k; i++) put16(buf + 2 * i, (unsigned short)s[i]);
        if (fwrite(buf, 1, (size_t)k * 2, w->f) != (size_t)k * 2) {
            w->err = 1;
            return -1;
        }
        w->bytes += (unsigned long)k * 2;
        s += k;
        n -= k;
    }
    return 0;
}

int wav_close(wav_writer* w)
{
    unsigned char b[4];
    int rc = 0;
    if (!w) return -1;
    if (w->err) rc = -1;
    if (!rc) {
        /* RIFF size na offsetu 4, data size na offsetu 40 */
        put32(b, 36 + w->bytes);
        if (fseek(w->f, 4, SEEK_SET) != 0 || fwrite(b, 1, 4, w->f) != 4) rc = -1;
        put32(b, w->bytes);
        if (!rc && (fseek(w->f, 40, SEEK_SET) != 0 || fwrite(b, 1, 4, w->f) != 4)) rc = -1;
    }
    if (fclose(w->f) != 0) rc = -1;
    free(w);
    return rc;
}
