/* picopad-sdimg: interface between main.c (pure C: arguments, host files) and
 * fat_ops.c (the SDK side: lib_fat.c). Built-in types only, no SDK headers. */
#ifndef SDIMG_FAT_OPS_H
#define SDIMG_FAT_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- provided by main.c: the image file, 512-byte sectors ---- */
unsigned img_sectors(void);                              /* size in sectors */
int img_read(unsigned sector, unsigned char* buf);       /* 0 = ok */
int img_write(unsigned sector, const unsigned char* buf);/* 0 = ok */

/* ---- provided by fat_ops.c; all return 0 = ok, otherwise a message in fo_error() ---- */
const char* fo_error(void);
int fo_format(void);                       /* DiskFormat (explicit fs and cluster size) + mount */
int fo_mount(void);
int fo_unmount(void);                      /* flushes */
int fo_check_name(const char* path);       /* every segment is a valid 8.3 name */
int fo_mkdir(const char* path);            /* creates it if missing (no error if exists) */
int fo_write_begin(const char* path);      /* creates/overwrites */
int fo_write(const void* data, unsigned size);
int fo_write_end(void);
int fo_read_begin(const char* path, unsigned* size);
int fo_read(void* data, unsigned size, unsigned* got);
int fo_read_end(void);
typedef void (*fo_list_cb)(void* ctx, int is_dir, const char* name, unsigned size);
int fo_list(const char* path, fo_list_cb cb, void* ctx);

#ifdef __cplusplus
}
#endif

#endif
