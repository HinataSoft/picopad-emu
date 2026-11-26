/* SD card image file (raw disk image, sectors of 512 B from offset 0). Pure C
   over FILE* with 64-bit offsets: no SDL, no windows.h, no SDK headers.
   Single user (the game thread); every write is flushed to the OS at once, so
   the data is in the file however the process ends (even TerminateProcess). */
#ifndef PICOPAD_PLATFORM_SDIMAGE_H
#define PICOPAD_PLATFORM_SDIMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#define SDIMAGE_SECTOR 512
#define SDIMAGE_MIN_BYTES (64ull * 1024)

/* Opens the image (read-write; read-only if the file is read-only: writes then
   fail). Returns 0 and no message, or -1 and a one-line message in err (does
   not exist, directory, size not a multiple of 512, below 64 KiB, ...). A
   previously opened image is closed first. */
int sdimage_open(const char* path, char* err, int errlen);

/* closes the image (no-op when none is open); the card is then "not inserted" */
void sdimage_close(void);

/* number of sectors, 0 = no image */
unsigned int sdimage_sectors(void);

/* 1 = OK, 0 = error (no image, sector out of range, I/O error; write: also read-only image) */
int sdimage_read(unsigned int sector, unsigned char* buf512);
int sdimage_write(unsigned int sector, const unsigned char* buf512);

#ifdef __cplusplus
}
#endif
#endif
