/* Import of a host directory into a FAT32 image (fatvol) and synchronisation of the
   image back to the directory, for --sddir. Uses windows.h (wide-char file APIs),
   no SDL, no SDK headers. One import at a time (the manifest is global); not thread-safe. */
#ifndef PICOPAD_PLATFORM_SDDIR_H
#define PICOPAD_PLATFORM_SDDIR_H

#include "fatvol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*sddir_warn_fn)(void* ctx, const char* msg); /* one line, no prefix, no newline */

/* Formats io, loads the tree of dir (8.3 rules, mtime -> stamp) and remembers a manifest of
   what was loaded (FAT path, host path, is_dir, host size + mtime, FAT size + stamp, content
   hash). 0 ok, -1 error (message in err: not a directory, card full, read error). */
int sddir_import(const char* dir, const fatvol_io* io, sddir_warn_fn warn, void* wctx, char* err, int errlen);

/* Applies the image back to the dir of the last sddir_import per the sync rules; returns the
   number of warnings issued. A second call is a no-op returning 0. */
int sddir_sync(const fatvol_io* io, sddir_warn_fn warn, void* wctx);

/* Writes the whole tree of the image into an empty or new dir (every entry is new).
   Returns the number of warnings (0 = ok), -1 if the image cannot be read. */
int sddir_export(const fatvol_io* io, const char* dir, sddir_warn_fn warn, void* wctx);

#ifdef __cplusplus
}
#endif
#endif
