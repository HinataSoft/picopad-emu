/* Sparse in-memory sector store, see sparsedisk.h. Pure C. */
#include "sparsedisk.h"

#include <stdlib.h>
#include <string.h>

#define BLOCK_BYTES ((size_t)SPARSE_BLOCK_SECTORS * SPARSE_SECTOR)

struct sparsedisk {
    unsigned int sectors;
    unsigned int nblocks;
    unsigned int allocated;
    unsigned char** blocks; /* NULL = not allocated (reads as zeros) */
};

sparsedisk* sparse_create(unsigned int sectors)
{
    sparsedisk* d;
    unsigned int nblocks = (sectors + SPARSE_BLOCK_SECTORS - 1) / SPARSE_BLOCK_SECTORS;

    d = (sparsedisk*)malloc(sizeof *d);
    if (!d) return NULL;
    d->blocks = (unsigned char**)calloc(nblocks ? nblocks : 1, sizeof *d->blocks);
    if (!d->blocks) {
        free(d);
        return NULL;
    }
    d->sectors = sectors;
    d->nblocks = nblocks;
    d->allocated = 0;
    return d;
}

void sparse_free(sparsedisk* d)
{
    unsigned int i;
    if (!d) return;
    for (i = 0; i < d->nblocks; i++)
        free(d->blocks[i]);
    free(d->blocks);
    free(d);
}

unsigned int sparse_sectors(const sparsedisk* d)
{
    return d->sectors;
}

int sparse_read(sparsedisk* d, unsigned int sector, unsigned char* buf512)
{
    unsigned char* block;
    size_t off;

    if (sector >= d->sectors) return 0;
    block = d->blocks[sector / SPARSE_BLOCK_SECTORS];
    off = (size_t)(sector % SPARSE_BLOCK_SECTORS) * SPARSE_SECTOR;
    if (block)
        memcpy(buf512, block + off, SPARSE_SECTOR);
    else
        memset(buf512, 0, SPARSE_SECTOR);
    return 1;
}

int sparse_write(sparsedisk* d, unsigned int sector, const unsigned char* buf512)
{
    unsigned int bi;
    unsigned char* block;
    size_t off;

    if (sector >= d->sectors) return 0;
    bi = sector / SPARSE_BLOCK_SECTORS;
    off = (size_t)(sector % SPARSE_BLOCK_SECTORS) * SPARSE_SECTOR;
    block = d->blocks[bi];
    if (!block) {
        /* zero writes do not allocate: a missing block already reads as zeros */
        unsigned int i;
        int nonzero = 0;
        for (i = 0; i < SPARSE_SECTOR; i++)
            if (buf512[i]) { nonzero = 1; break; }
        if (!nonzero) return 1;
        block = (unsigned char*)calloc(1, BLOCK_BYTES);
        if (!block) return 0;
        d->blocks[bi] = block;
        d->allocated++;
    }
    memcpy(block + off, buf512, SPARSE_SECTOR);
    return 1;
}

unsigned int sparse_blocks_allocated(const sparsedisk* d)
{
    return d->allocated;
}
