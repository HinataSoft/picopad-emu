/* The SD card of a game .exe: an image file (--sd) or a 1 GiB sparse in-memory image built
   from a host directory (--sddir). Serializes the game thread (sector access) against the
   shutdown (close / sync from the main, console or watchdog thread). Uses windows.h
   (CRITICAL_SECTION), no SDL, no SDK headers. */
#ifndef PICOPAD_PLATFORM_SDCARD_H
#define PICOPAD_PLATFORM_SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

/* --sd: 0 ok, -1 error (one-line message in err) */
int sdcard_open_image(const char* path, char* err, int errlen);

/* --sddir: formats the in-memory card and loads dir. 0 ok, -1 error (message in err; the exe
   exits with code 2). Warnings go to stderr as "<game_name>: --sddir: <text>". */
int sdcard_open_dir(const char* dir, const char* game_name, char* err, int errlen);

/* 0 = no card */
unsigned int sdcard_sectors(void);

/* 1 = ok, 0 = error (no card, out of range, I/O error) */
int sdcard_read(unsigned int sector, unsigned char* buf512);
int sdcard_write(unsigned int sector, const unsigned char* buf512);

/* Ends the use of the card: --sd closes the image file; --sddir syncs the image back to the
   directory (once). Waits for a sector operation in progress, then keeps the card lock for
   good, so the game thread blocks at its next card access. Safe from any thread and any
   number of times; a second caller waits (bounded) for the first one to finish. */
void sdcard_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif
