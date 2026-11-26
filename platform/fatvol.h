/* Minimal FAT32 formatter, writer and reader for --sddir. Pure C: no SDL, no
   windows.h, no SDK headers. The volume layout (MBR + one FAT32 partition, BPB,
   FSInfo) mirrors PicoLibSDK lib_fat DiskFormat(FS_FAT32, clust, True, ...), so
   lib_fat mounts what is written here and this code reads what lib_fat writes.
   Not thread-safe: the owner serializes access. */
#ifndef PICOPAD_PLATFORM_FATVOL_H
#define PICOPAD_PLATFORM_FATVOL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* ctx;
    unsigned int sectors;
    int (*read)(void* ctx, unsigned int sector, unsigned char* buf512);        /* 1 ok */
    int (*write)(void* ctx, unsigned int sector, const unsigned char* buf512); /* 1 ok */
} fatvol_io;

typedef struct {
    unsigned short date, time;
} fat_stamp; /* FAT-encoded, local time */

typedef struct fatvol fatvol;

/* MBR + one FAT32 partition, layout as lib_fat DiskFormat(FS_FAT32, clust, True, ...):
   clust starts at 32 sectors and is halved until DiskFormat accepts the cluster
   count (more than 0xFFF8 clusters); error if not reachable. NULL + err on error. */
fatvol* fatvol_format(const fatvol_io* io, char* err, int errlen);
/* mounts an existing FAT32 volume (MBR partition or superfloppy); other FAT types -> NULL + err */
fatvol* fatvol_open(const fatvol_io* io, char* err, int errlen);
const char* fatvol_error(const fatvol* v); /* last error message */
/* writes the FAT and FSInfo (free count, last allocated); 0 ok, nonzero = error (message in
   fatvol_error). Writing is done once; a later fatvol_close does not repeat a successful flush. */
int fatvol_flush(fatvol* v);
int fatvol_close(fatvol* v);               /* fatvol_flush + frees; returns the flush result (0 ok) */

/* paths: uppercase 8.3 segments separated by '/', no leading '/', root = "".
   All of these return 0 = ok, nonzero = error (message in fatvol_error). */
int fatvol_mkdir(fatvol* v, const char* path, fat_stamp t); /* parent must exist */
/* reserves clusters (contiguous when possible); "SD card full" if no room */
int fatvol_file_begin(fatvol* v, const char* path, unsigned int size, fat_stamp t);
int fatvol_file_write(fatvol* v, const void* data, unsigned int n); /* sum of n must equal size */
int fatvol_file_end(fatvol* v);
int fatvol_delete(fatvol* v, const char* path); /* file or empty dir: entry 0xE5, chain freed */

typedef struct {
    const char* path; /* as above, valid during the callback */
    int is_dir;
    unsigned int size;
    fat_stamp stamp; /* write date/time */
    unsigned int first_cluster;
} fatvol_entry;
/* pre-order walk (a directory before its contents); skips ".", "..", volume label,
   deleted and LFN entries. cb returns 0 = go on, nonzero = stop (returned). -1 = read error / bad chain. */
int fatvol_walk(fatvol* v, int (*cb)(void* ctx, const fatvol_entry* e), void* ctx);
/* follows the cluster chain of a file; sink returns 0 = go on. -1 = bad chain / read error. */
int fatvol_read_file(fatvol* v, const fatvol_entry* e,
                     int (*sink)(void* ctx, const void* data, unsigned int n), void* ctx);

#ifdef __cplusplus
}
#endif
#endif
