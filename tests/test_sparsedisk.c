/* Test radeho sektoroveho uloziste: nulova cteni, alokace bloku az pri zapisu
   nenulovych dat, hranice rozsahu, prepis nulami, sparse_free(NULL). */
#include "check.h"
#include "../platform/sparsedisk.h"

#include <string.h>

#define TOTAL 2097152u /* 1 GiB / 512 B */

static void fill(unsigned char* buf, unsigned int seed)
{
    unsigned int i;
    for (i = 0; i < SPARSE_SECTOR; i++)
        buf[i] = (unsigned char)(i * 7u + seed);
}

static int is_zero(const unsigned char* buf)
{
    unsigned int i;
    for (i = 0; i < SPARSE_SECTOR; i++)
        if (buf[i] != 0) return 0;
    return 1;
}

int main(void)
{
    unsigned char buf[SPARSE_SECTOR];
    unsigned char pat[SPARSE_SECTOR];
    sparsedisk* d;

    sparse_free(NULL); /* no-op, nesmi padnout */

    d = sparse_create(TOTAL);
    CHECK(d != NULL);
    if (!d) return 1;
    CHECK_EQ(sparse_sectors(d), TOTAL);
    CHECK_EQ(sparse_blocks_allocated(d), 0);

    /* nealokovane sektory (prvni, posledni) cteni jako nuly */
    memset(buf, 0xAA, sizeof buf);
    CHECK_EQ(sparse_read(d, 0, buf), 1);
    CHECK(is_zero(buf));
    memset(buf, 0xAA, sizeof buf);
    CHECK_EQ(sparse_read(d, TOTAL - 1, buf), 1);
    CHECK(is_zero(buf));
    CHECK_EQ(sparse_read(d, TOTAL, buf), 0);

    /* zapis nul nealokuje blok */
    memset(buf, 0, sizeof buf);
    CHECK_EQ(sparse_write(d, 1000, buf), 1);
    CHECK_EQ(sparse_blocks_allocated(d), 0);

    /* zapis nenulovych dat alokuje blok; zpetne cteni je stejne */
    fill(pat, 1); /* bajt i*7+1 */
    CHECK_EQ(sparse_write(d, 1000, pat), 1);
    CHECK_EQ(sparse_blocks_allocated(d), 1);
    memset(buf, 0, sizeof buf);
    CHECK_EQ(sparse_read(d, 1000, buf), 1);
    CHECK(memcmp(buf, pat, SPARSE_SECTOR) == 0);

    /* sektor ve stejnem bloku (1001) je nulovy; dalsi blok (1000+128) je nulovy a nealokuje */
    memset(buf, 0xAA, sizeof buf);
    CHECK_EQ(sparse_read(d, 1001, buf), 1);
    CHECK(is_zero(buf));
    memset(buf, 0xAA, sizeof buf);
    CHECK_EQ(sparse_read(d, 1000 + SPARSE_BLOCK_SECTORS, buf), 1);
    CHECK(is_zero(buf));
    CHECK_EQ(sparse_blocks_allocated(d), 1);

    /* posledni sektor: dalsi blok; mimo rozsah selze */
    CHECK_EQ(sparse_write(d, TOTAL - 1, pat), 1);
    CHECK_EQ(sparse_blocks_allocated(d), 2);
    CHECK_EQ(sparse_write(d, TOTAL, pat), 0);
    CHECK_EQ(sparse_blocks_allocated(d), 2);

    /* prepis sektoru nulami: cte nuly (blok muze zustat alokovany) */
    memset(buf, 0, sizeof buf);
    CHECK_EQ(sparse_write(d, 1000, buf), 1);
    memset(buf, 0xAA, sizeof buf);
    CHECK_EQ(sparse_read(d, 1000, buf), 1);
    CHECK(is_zero(buf));

    sparse_free(d);
    return CHECK_DONE();
}
