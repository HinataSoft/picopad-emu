/* The SD card of a game .exe (--sd / --sddir), see sdcard.h. */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "sdcard.h"
#include "sddir.h"
#include "sdimage.h"
#include "sparsedisk.h"

#define DIR_SECTORS 2097152u        /* 1 GiB */
#define SHUTDOWN_WAIT_MS 120000     /* a second shutdown caller waits at most this long */

enum { MODE_NONE = 0, MODE_IMAGE, MODE_DIR };

static int Mode = MODE_NONE;
static CRITICAL_SECTION Lock;
static int LockReady;
static HANDLE DoneEvent;            /* manual reset, set when the first shutdown has finished */
static volatile LONG ShutdownStarted;
static unsigned int Sectors;

static sparsedisk* Disk;
static fatvol_io Io;
static char GameName[64];

static int io_read(void* ctx, unsigned int sector, unsigned char* buf)
{
    return sparse_read((sparsedisk*)ctx, sector, buf);
}

static int io_write(void* ctx, unsigned int sector, const unsigned char* buf)
{
    return sparse_write((sparsedisk*)ctx, sector, buf);
}

static void warn_to_stderr(void* ctx, const char* msg)
{
    (void)ctx;
    fprintf(stderr, "%s: --sddir: %s\n", GameName, msg);
    fflush(stderr);
}

static int init_sync(char* err, int errlen)
{
    if (LockReady) return 0;
    DoneEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (DoneEvent == NULL)
    {
        if (err && errlen > 0) snprintf(err, (size_t)errlen, "cannot create synchronization object");
        return -1;
    }
    InitializeCriticalSection(&Lock);
    LockReady = 1;
    return 0;
}

int sdcard_open_image(const char* path, char* err, int errlen)
{
    if (init_sync(err, errlen) != 0) return -1;
    if (sdimage_open(path, err, errlen) != 0) return -1;
    Mode = MODE_IMAGE;
    Sectors = sdimage_sectors();
    return 0;
}

int sdcard_open_dir(const char* dir, const char* game_name, char* err, int errlen)
{
    if (init_sync(err, errlen) != 0) return -1;
    snprintf(GameName, sizeof(GameName), "%s", game_name ? game_name : "picopad");
    Disk = sparse_create(DIR_SECTORS);
    if (Disk == NULL)
    {
        if (err && errlen > 0) snprintf(err, (size_t)errlen, "out of memory");
        return -1;
    }
    Io.ctx = Disk;
    Io.sectors = DIR_SECTORS;
    Io.read = io_read;
    Io.write = io_write;
    if (sddir_import(dir, &Io, warn_to_stderr, NULL, err, errlen) != 0)
    {
        sparse_free(Disk);
        Disk = NULL;
        return -1;
    }
    Mode = MODE_DIR;
    Sectors = DIR_SECTORS;
    return 0;
}

unsigned int sdcard_sectors(void)
{
    return Mode == MODE_NONE ? 0 : Sectors;
}

int sdcard_read(unsigned int sector, unsigned char* buf512)
{
    int ok;
    if (Mode == MODE_NONE) return 0;
    EnterCriticalSection(&Lock);
    ok = (Mode == MODE_IMAGE) ? sdimage_read(sector, buf512) : sparse_read(Disk, sector, buf512);
    LeaveCriticalSection(&Lock);
    return ok;
}

int sdcard_write(unsigned int sector, const unsigned char* buf512)
{
    int ok;
    if (Mode == MODE_NONE) return 0;
    EnterCriticalSection(&Lock);
    ok = (Mode == MODE_IMAGE) ? sdimage_write(sector, buf512) : sparse_write(Disk, sector, buf512);
    LeaveCriticalSection(&Lock);
    return ok;
}

void sdcard_shutdown(void)
{
    if (Mode == MODE_NONE) return;
    if (InterlockedExchange(&ShutdownStarted, 1) != 0)
    {
        /* somebody else is (or was) shutting down: wait for it, never forever */
        WaitForSingleObject(DoneEvent, SHUTDOWN_WAIT_MS);
        return;
    }
    /* waits for the sector operation in progress; the lock is never released, so the game
       thread stops at its next card access */
    EnterCriticalSection(&Lock);
    if (Mode == MODE_IMAGE) sdimage_close();
    else sddir_sync(&Io, warn_to_stderr, NULL);
    fflush(stderr);
    SetEvent(DoneEvent);
}
