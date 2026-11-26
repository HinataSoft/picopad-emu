/* Sparse in-memory sector store for --sddir (the SD card of 1 GiB served from
   a host directory). Pure C: no SDL, no windows.h, no SDK headers. No lock:
   the owner (sdcard, Task 4) serializes access. Blocks of 64 KiB are allocated
   only when a non-zero sector is written; a block that was never allocated
   reads as zeros. */
#ifndef PICOPAD_PLATFORM_SPARSEDISK_H
#define PICOPAD_PLATFORM_SPARSEDISK_H

#ifdef __cplusplus
extern "C" {
#endif

#define SPARSE_SECTOR 512
#define SPARSE_BLOCK_SECTORS 128 /* 64 KiB */

typedef struct sparsedisk sparsedisk;

/* NULL = out of memory; nothing is allocated per block yet */
sparsedisk* sparse_create(unsigned int sectors);

/* NULL is a no-op */
void sparse_free(sparsedisk* d);

unsigned int sparse_sectors(const sparsedisk* d);

/* 1 = OK, 0 = sector out of range */
int sparse_read(sparsedisk* d, unsigned int sector, unsigned char* buf512);

/* 1 = OK, 0 = sector out of range or out of memory */
int sparse_write(sparsedisk* d, unsigned int sector, const unsigned char* buf512);

unsigned int sparse_blocks_allocated(const sparsedisk* d);

#ifdef __cplusplus
}
#endif
#endif
