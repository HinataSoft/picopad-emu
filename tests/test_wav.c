/* Test zapisu WAV: hlavicka a velikosti po wav_close. */
#include "check.h"
#include "../platform/wav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long le32(const unsigned char* p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) |
           ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}
static unsigned le16(const unsigned char* p) { return p[0] | (p[1] << 8); }

int main(void)
{
    short s[1000];
    unsigned char b[44 + 2000 + 16];
    FILE* f;
    size_t n;
    int i;
    wav_writer* w;

    for (i = 0; i < 1000; i++) s[i] = (short)(i * 37 - 18000);

    w = wav_open("test_out.wav", 22050);
    CHECK(w != NULL);
    if (!w) return 1;
    CHECK_EQ(wav_write(w, s, 400), 0);
    CHECK_EQ(wav_write(w, s + 400, 600), 0);
    CHECK_EQ(wav_close(w), 0);

    f = fopen("test_out.wav", "rb");
    CHECK(f != NULL);
    if (!f) return 1;
    n = fread(b, 1, sizeof b, f);
    fclose(f);
    CHECK_EQ(n, 44 + 2000);
    CHECK(memcmp(b, "RIFF", 4) == 0);
    CHECK_EQ(le32(b + 4), 2036);
    CHECK(memcmp(b + 8, "WAVEfmt ", 8) == 0);
    CHECK_EQ(le32(b + 16), 16);
    CHECK_EQ(le16(b + 20), 1);
    CHECK_EQ(le16(b + 22), 1);
    CHECK_EQ(le32(b + 24), 22050);
    CHECK_EQ(le32(b + 28), 44100);
    CHECK_EQ(le16(b + 32), 2);
    CHECK_EQ(le16(b + 34), 16);
    CHECK(memcmp(b + 36, "data", 4) == 0);
    CHECK_EQ(le32(b + 40), 2000);
    for (i = 0; i < 1000; i++)
        if ((short)le16(b + 44 + 2 * i) != s[i]) { CHECK(0); break; }

    CHECK(wav_open("no_such_dir/x.wav", 22050) == NULL);
    return CHECK_DONE();
}
