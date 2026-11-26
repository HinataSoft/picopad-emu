/* Minimal FAT32 formatter, writer and reader (see fatvol.h).

   The geometry code mirrors PicoLibSDK _lib/src/lib_fat.c:
   - DiskFormat() (CHS heads/cylinders, MBR, BPB, FSInfo, first FAT sector, root)
   - Disk_CheckFS() and DiskMount() (what is required of the boot sector)
   - FileMkDir() for the "." / ".." entries of a new directory. */
#include "fatvol.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECT 512u
#define DIRSZ 32u
#define MAX_FAT12 0xFF5u
#define MAX_FAT16 0xFFF8u
#define MAX_FAT32 0x0FFFFFF5u
#define EOC 0x0FFFFFFFu
#define MASK28 0x0FFFFFFFu
#define MAX_DEPTH 40

#define ATTR_DIR 0x10
#define ATTR_ARCH 0x20
#define ATTR_VOL 0x08
#define ATTR_LFN 0x0F

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

struct fatvol {
    fatvol_io io;
    char err[256];
    u32 hsect;     /* partition start sector */
    u32 clustsz;   /* sectors per cluster */
    u32 fatnum;    /* number of FAT copies (1 or 2) */
    u32 fatsz;     /* sectors per FAT */
    u32 fatbase;   /* absolute sector of the first FAT */
    u32 database;  /* absolute sector of cluster 2 */
    u32 clustnum;  /* number of data clusters */
    u32 rootcl;    /* root directory cluster */
    u32 fsinfo;    /* absolute sector of FSInfo, 0 = none */
    u32* fat;      /* whole FAT in memory, fatsz * 128 entries */
    u32 fat_lo, fat_hi; /* dirty FAT sector range (lo > hi = clean) */
    u32 free_cnt;
    u32 last_alloc;
    int modified;
    /* file being written */
    int f_active;
    u32 f_remaining, f_cl, f_secidx, f_bufn;
    u8 f_buf[SECT];
};

typedef struct {
    u32 sec;
    u32 off;
} dloc;

/* ---------------------------------------------------------------- helpers */

static void seterr(char* dst, int n, const char* fmt, ...)
{
    va_list ap;
    if (!dst || n <= 0) return;
    va_start(ap, fmt);
    vsnprintf(dst, (size_t)n, fmt, ap);
    va_end(ap);
    dst[n - 1] = 0;
}

static int fail(fatvol* v, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(v->err, sizeof(v->err), fmt, ap);
    va_end(ap);
    return 1;
}

static u32 g16(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8); }
static u32 g32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static void p16(u8* p, u32 x)
{
    p[0] = (u8)x;
    p[1] = (u8)(x >> 8);
}
static void p32(u8* p, u32 x)
{
    p[0] = (u8)x;
    p[1] = (u8)(x >> 8);
    p[2] = (u8)(x >> 16);
    p[3] = (u8)(x >> 24);
}

static int rd(fatvol* v, u32 sec, u8* buf)
{
    if (sec >= v->io.sectors || !v->io.read(v->io.ctx, sec, buf)) return fail(v, "read error at sector %u", sec);
    return 0;
}
static int wr(fatvol* v, u32 sec, const u8* buf)
{
    if (sec >= v->io.sectors || !v->io.write(v->io.ctx, sec, buf)) return fail(v, "write error at sector %u", sec);
    return 0;
}

static int valid_cl(const fatvol* v, u32 c) { return c >= 2 && c < v->clustnum + 2; }
static u32 cl_lba(const fatvol* v, u32 c) { return v->database + (c - 2) * v->clustsz; }
static u32 fat_get(const fatvol* v, u32 c) { return v->fat[c] & MASK28; }

static void fat_set(fatvol* v, u32 c, u32 val)
{
    u32 s = c / 128;
    v->fat[c] = val;
    if (v->fat_lo > v->fat_hi) v->fat_lo = v->fat_hi = s;
    else {
        if (s < v->fat_lo) v->fat_lo = s;
        if (s > v->fat_hi) v->fat_hi = s;
    }
    v->modified = 1;
}

static int flush_fat(fatvol* v)
{
    u32 s, k;
    u8 buf[SECT];
    if (v->fat_lo > v->fat_hi) return 0;
    for (s = v->fat_lo; s <= v->fat_hi; s++) {
        u32 i;
        for (i = 0; i < 128; i++) p32(buf + i * 4, v->fat[s * 128 + i]);
        for (k = 0; k < v->fatnum; k++)
            if (wr(v, v->fatbase + k * v->fatsz + s, buf)) return 1;
    }
    v->fat_lo = 1;
    v->fat_hi = 0;
    return 0;
}

static int zero_cluster(fatvol* v, u32 c)
{
    u8 z[SECT];
    u32 i;
    memset(z, 0, sizeof(z));
    for (i = 0; i < v->clustsz; i++)
        if (wr(v, cl_lba(v, c) + i, z)) return 1;
    return 0;
}

/* ---------------------------------------------------------------- allocation */

/* allocates a chain of n >= 1 clusters: first-fit contiguous run, else the first
   n free clusters (chained in order); returns 0 and the first cluster */
static int alloc_chain(fatvol* v, u32 n, u32* first)
{
    u32 c, run = 0, start = 0, i;
    if (n == 0 || n > v->free_cnt) return fail(v, "SD card full");
    for (c = 2; c < v->clustnum + 2; c++) {
        if (fat_get(v, c) == 0) {
            if (run == 0) start = c;
            if (++run == n) break;
        } else
            run = 0;
    }
    if (run == n) {
        for (i = 0; i + 1 < n; i++) fat_set(v, start + i, start + i + 1);
        fat_set(v, start + n - 1, EOC);
        v->last_alloc = start + n - 1;
    } else {
        u32 prev = 0, left = n;
        start = 0;
        for (c = 2; c < v->clustnum + 2 && left; c++) {
            if (fat_get(v, c) != 0) continue;
            if (prev) fat_set(v, prev, c);
            else start = c;
            fat_set(v, c, EOC); /* provisional end, replaced when linked */
            prev = c;
            left--;
        }
        if (left) return fail(v, "SD card full"); /* cannot happen: free_cnt checked */
        v->last_alloc = prev;
    }
    v->free_cnt -= n;
    *first = start;
    return 0;
}

static void free_chain(fatvol* v, u32 c)
{
    u32 guard = v->clustnum + 2;
    while (valid_cl(v, c) && guard--) {
        u32 nx = fat_get(v, c);
        fat_set(v, c, 0);
        v->free_cnt++;
        if (nx >= 0x0FFFFFF8u || nx == 0) break;
        c = nx;
    }
}

/* ---------------------------------------------------------------- names and paths */

/* converts one path segment to 11 space padded upper case characters;
   same rules as fo_check_name (base 1-8, ext 0-3, one dot, not at either end) */
static int seg_name(const char* s, u32 len, char out[11])
{
    u32 i, base = 0, ext = 0;
    int dot = 0;
    memset(out, ' ', 11);
    if (len == 0) return 1;
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '.') {
            if (dot || i == 0 || i == len - 1) return 1;
            dot = 1;
            continue;
        }
        if (ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - 32);
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch && strchr("!#$%&'()-@^_`{}~", ch))))
            return 1;
        if (!dot) {
            if (base >= 8) return 1;
            out[base++] = (char)ch;
        } else {
            if (ext >= 3) return 1;
            out[8 + ext++] = (char)ch;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- directories */

/* searches a directory chain: 0 found (loc, ent), 1 not found, -1 error */
static int dir_find(fatvol* v, u32 dircl, const char name[11], dloc* loc, u8* ent)
{
    u32 c = dircl, guard = v->clustnum + 2;
    u8 b[SECT];
    while (valid_cl(v, c) && guard--) {
        u32 s, i;
        for (s = 0; s < v->clustsz; s++) {
            if (rd(v, cl_lba(v, c) + s, b)) return -1;
            for (i = 0; i < SECT; i += DIRSZ) {
                if (b[i] == 0) return 1;
                if (b[i] == 0xE5 || b[i + 11] == ATTR_LFN) continue;
                if (memcmp(b + i, name, 11) == 0) {
                    if (loc) {
                        loc->sec = cl_lba(v, c) + s;
                        loc->off = i;
                    }
                    if (ent) memcpy(ent, b + i, DIRSZ);
                    return 0;
                }
            }
        }
        c = fat_get(v, c);
        if (c >= 0x0FFFFFF8u) return 1;
    }
    return fail(v, "bad directory chain"), -1;
}

/* finds a free slot in a directory, extending it by a cluster when full */
static int dir_slot(fatvol* v, u32 dircl, dloc* loc)
{
    u32 c = dircl, last = dircl, guard = v->clustnum + 2;
    u8 b[SECT];
    while (valid_cl(v, c) && guard--) {
        u32 s, i;
        for (s = 0; s < v->clustsz; s++) {
            if (rd(v, cl_lba(v, c) + s, b)) return 1;
            for (i = 0; i < SECT; i += DIRSZ)
                if (b[i] == 0 || b[i] == 0xE5) {
                    loc->sec = cl_lba(v, c) + s;
                    loc->off = i;
                    return 0;
                }
        }
        last = c;
        c = fat_get(v, c);
        if (c >= 0x0FFFFFF8u) break;
    }
    {
        u32 nc;
        if (!valid_cl(v, last)) return fail(v, "bad directory chain");
        if (alloc_chain(v, 1, &nc)) return 1;
        if (zero_cluster(v, nc)) return 1;
        fat_set(v, last, nc);
        loc->sec = cl_lba(v, nc);
        loc->off = 0;
    }
    return 0;
}

static int put_entry(fatvol* v, const dloc* loc, const char name[11], u8 attr, fat_stamp t, u32 first, u32 size)
{
    u8 b[SECT];
    u8* e;
    if (rd(v, loc->sec, b)) return 1;
    e = b + loc->off;
    memset(e, 0, DIRSZ);
    memcpy(e, name, 11);
    e[11] = attr;
    p16(e + 14, t.time);
    p16(e + 16, t.date);
    p16(e + 18, t.date);
    p16(e + 20, first >> 16);
    p16(e + 22, t.time);
    p16(e + 24, t.date);
    p16(e + 26, first & 0xFFFF);
    p32(e + 28, size);
    return wr(v, loc->sec, b);
}

/* resolves all but the last segment: parent directory cluster and the last name */
static int resolve_parent(fatvol* v, const char* path, u32* parent, char name[11])
{
    u32 cl = v->rootcl;
    const char* p = path;
    int depth = 0;
    if (!path || !*path) return fail(v, "empty path");
    for (;;) {
        const char* sl = strchr(p, '/');
        u32 len = sl ? (u32)(sl - p) : (u32)strlen(p);
        char nm[11];
        if (seg_name(p, len, nm)) return fail(v, "invalid 8.3 name in path \"%s\"", path);
        if (!sl) {
            memcpy(name, nm, 11);
            *parent = cl;
            return 0;
        }
        {
            dloc loc;
            u8 e[DIRSZ];
            int r = dir_find(v, cl, nm, &loc, e);
            if (r < 0) return 1;
            if (r > 0) return fail(v, "directory not found in path \"%s\"", path);
            if (!(e[11] & ATTR_DIR)) return fail(v, "not a directory in path \"%s\"", path);
            cl = g16(e + 26) | (g16(e + 20) << 16);
            if (cl == 0) cl = v->rootcl;
        }
        if (++depth > MAX_DEPTH) return fail(v, "path too deep");
        p = sl + 1;
    }
}

/* ---------------------------------------------------------------- mount */

/* mirrors Disk_CheckFS(): 0 = not a FAT boot sector, 1 = FAT12/16, 2 = FAT32 */
static int check_boot(const u8* b)
{
    u32 fatsize, fatnum, clustsize, rootnum, totsect, ressect, syssect, clustnum;
    if (g16(b + 510) != 0xAA55) return 0;
    if (memcmp(b + 0x52, "FAT32", 5) != 0 && memcmp(b + 0x36, "FAT", 3) != 0) return 0;
    if (g16(b + 11) != SECT) return 0;
    fatsize = g16(b + 22);
    if (fatsize == 0) fatsize = g32(b + 36);
    if (fatsize == 0) return 0;
    fatnum = b[16];
    if (fatnum != 1 && fatnum != 2) return 0;
    fatsize *= fatnum;
    clustsize = b[13];
    if (clustsize == 0 || (clustsize & (clustsize - 1))) return 0;
    rootnum = g16(b + 17);
    if (rootnum & 15) return 0;
    totsect = g16(b + 19);
    if (totsect == 0) totsect = g32(b + 32);
    ressect = g16(b + 14);
    if (ressect == 0) return 0;
    syssect = ressect + fatsize + rootnum / 16;
    if (totsect < syssect) return 0;
    clustnum = (totsect - syssect) / clustsize;
    if (clustnum == 0 || clustnum > MAX_FAT32) return 0;
    return (clustnum > MAX_FAT16) ? 2 : 1;
}

/* parses the boot sector at hsect (already validated as FAT32) and loads the FAT */
static fatvol* mount_at(const fatvol_io* io, u32 hsect, const u8* b, char* err, int errlen)
{
    fatvol* v = (fatvol*)calloc(1, sizeof(fatvol));
    u32 totsect, ressect, syssect, need, s, c;
    if (!v) {
        seterr(err, errlen, "out of memory");
        return NULL;
    }
    v->io = *io;
    v->hsect = hsect;
    v->clustsz = b[13];
    v->fatnum = b[16];
    v->fatsz = g16(b + 22);
    if (v->fatsz == 0) v->fatsz = g32(b + 36);
    totsect = g16(b + 19);
    if (totsect == 0) totsect = g32(b + 32);
    ressect = g16(b + 14);
    if (g16(b + 17) != 0) {
        seterr(err, errlen, "FAT32 volume with a root directory table");
        goto bad;
    }
    syssect = ressect + v->fatsz * v->fatnum;
    v->clustnum = (totsect - syssect) / v->clustsz;
    v->fatbase = hsect + ressect;
    v->database = hsect + syssect;
    v->rootcl = g32(b + 44);
    need = (v->clustnum + 2) * 4;
    if (v->fatsz < (need + SECT - 1) / SECT) {
        seterr(err, errlen, "FAT too small for the volume");
        goto bad;
    }
    if (!valid_cl(v, v->rootcl)) {
        seterr(err, errlen, "invalid root directory cluster");
        goto bad;
    }
    if (hsect + totsect > io->sectors) {
        seterr(err, errlen, "volume is larger than the storage");
        goto bad;
    }
    if (g16(b + 48) == 1) v->fsinfo = hsect + 1;
    v->fat = (u32*)calloc((size_t)v->fatsz * 128, sizeof(u32));
    if (!v->fat) {
        seterr(err, errlen, "out of memory");
        goto bad;
    }
    {
        u8 sec[SECT];
        for (s = 0; s < v->fatsz; s++) {
            u32 i;
            if (rd(v, v->fatbase + s, sec)) {
                seterr(err, errlen, "%s", v->err);
                goto bad;
            }
            for (i = 0; i < 128; i++) v->fat[s * 128 + i] = g32(sec + i * 4);
        }
        v->last_alloc = 2;
        if (v->fsinfo && rd(v, v->fsinfo, sec) == 0 && g32(sec) == 0x41615252u && g32(sec + 0x1E4) == 0x61417272u) {
            u32 l = g32(sec + 0x1EC);
            if (valid_cl(v, l)) v->last_alloc = l;
        }
    }
    for (c = 2; c < v->clustnum + 2; c++)
        if (fat_get(v, c) == 0) v->free_cnt++;
    v->fat_lo = 1;
    v->fat_hi = 0;
    return v;
bad:
    free(v->fat);
    free(v);
    return NULL;
}

fatvol* fatvol_open(const fatvol_io* io, char* err, int errlen)
{
    u8 b[SECT];
    u32 hsect = 0;
    int fs;
    fatvol tmp;
    if (!io || !io->read || !io->write || io->sectors == 0) {
        seterr(err, errlen, "invalid storage");
        return NULL;
    }
    memset(&tmp, 0, sizeof(tmp));
    tmp.io = *io;
    if (rd(&tmp, 0, b)) {
        seterr(err, errlen, "%s", tmp.err);
        return NULL;
    }
    if (g16(b + 510) != 0xAA55) {
        seterr(err, errlen, "no boot signature: the storage is not formatted");
        return NULL;
    }
    fs = check_boot(b);
    if (fs == 0) { /* valid MBR? take the first FAT partition */
        u32 strt[4], i;
        for (i = 0; i < 4; i++) strt[i] = (b[446 + i * 16 + 4] == 0) ? 0 : g32(b + 446 + i * 16 + 8);
        for (i = 0; i < 4; i++) {
            if (strt[i] == 0) continue;
            if (rd(&tmp, strt[i], b)) {
                seterr(err, errlen, "%s", tmp.err);
                return NULL;
            }
            fs = check_boot(b);
            if (fs) {
                hsect = strt[i];
                break;
            }
        }
    }
    if (fs == 0) {
        seterr(err, errlen, "no FAT file system found");
        return NULL;
    }
    if (fs != 2) {
        seterr(err, errlen, "the volume is FAT12/FAT16, only FAT32 is supported");
        return NULL;
    }
    return mount_at(io, hsect, b, err, errlen);
}

/* ---------------------------------------------------------------- format */

fatvol* fatvol_format(const fatvol_io* io, char* err, int errlen)
{
    fatvol tmp;
    u8 b[SECT], fi[SECT], z[SECT], mbr[SECT];
    u32 total, media, hsect = 63, cyl1, ecyl, clust, clustnum = 0, res = 32, fatsize = 0, i;
    u32 heads = 16;
    if (!io || !io->read || !io->write) {
        seterr(err, errlen, "invalid storage");
        return NULL;
    }
    memset(&tmp, 0, sizeof(tmp));
    tmp.io = *io;
    total = io->sectors;
    if (total < 10 || total > 0xFFFFFFF5u) {
        seterr(err, errlen, "unsupported storage size");
        return NULL;
    }
    /* CHS geometry (DiskFormat): 63 sectors per track, max 1024 cylinders */
    for (;;) {
        cyl1 = heads * 63;
        if ((total + cyl1 - 1) / cyl1 <= 1024u) break;
        if (heads == 128) {
            heads = 255;
            break;
        }
        heads *= 2;
    }
    ecyl = (total + cyl1 - 1) / cyl1 - 1;
    media = total - 63; /* first track reserved for the MBR */

    /* cluster size: 32 sectors halved until DiskFormat(FS_FAT32, clust, ...) accepts */
    for (clust = 32;; clust >>= 1) {
        if (clust == 0) {
            seterr(err, errlen, "storage too small for FAT32 (needs more than %u clusters)", MAX_FAT16);
            return NULL;
        }
        clustnum = media / clust;
        fatsize = (clustnum * 4 + 8 + SECT - 1) / SECT;
        clustnum = (media - res - fatsize) / clust;
        fatsize = (clustnum * 4 + 8 + SECT - 1) / SECT;
        if (clustnum > MAX_FAT16) {
            if (clustnum > MAX_FAT32) clustnum = MAX_FAT32;
            break;
        }
    }
    media = res + fatsize + clustnum * clust; /* partition size */

    /* MBR */
    memset(mbr, 0, sizeof(mbr));
    mbr[446 + 1] = 1;                                  /* start head */
    mbr[446 + 2] = 1;                                  /* start sector */
    mbr[446 + 3] = 0;                                  /* start cylinder */
    mbr[446 + 4] = 12;                                 /* FAT32 LBA */
    mbr[446 + 5] = (u8)(heads - 1);                    /* end head */
    mbr[446 + 6] = (u8)(63 | ((ecyl >> 2) & 0xC0));    /* end sector + cyl high */
    mbr[446 + 7] = (u8)ecyl;                           /* end cylinder low */
    p32(mbr + 446 + 8, 63);
    p32(mbr + 446 + 12, media);
    p16(mbr + 510, 0xAA55);

    /* boot sector */
    memset(b, 0, sizeof(b));
    b[0] = 0xEB;
    b[1] = 0xFE;
    b[2] = 0x90;
    memcpy(b + 3, "MSDOS5.0", 8);
    p16(b + 11, SECT);
    b[13] = (u8)clust;
    p16(b + 14, res);
    b[16] = 1; /* one FAT, as DiskFormat */
    p16(b + 17, 0);
    if (media < 0x10000u) p16(b + 19, media);
    else p32(b + 32, media);
    b[21] = 0xF8;
    p16(b + 24, 63);
    p16(b + 26, heads);
    p32(b + 28, hsect);
    p32(b + 36, fatsize);
    p32(b + 44, 2);   /* root cluster */
    p16(b + 48, 1);   /* FSInfo sector */
    p16(b + 50, 6);   /* backup boot sector */
    b[64] = 0x80;
    b[66] = 0x29;
    memcpy(b + 71, "NO NAME    ", 11);
    memcpy(b + 82, "FAT32   ", 8);
    p16(b + 510, 0xAA55);

    /* FSInfo */
    memset(fi, 0, sizeof(fi));
    p32(fi, 0x41615252u);
    p32(fi + 0x1E4, 0x61417272u);
    p32(fi + 0x1E8, clustnum - 1);
    p32(fi + 0x1EC, 2);
    p16(fi + 510, 0xAA55);

    memset(z, 0, sizeof(z));
    if (wr(&tmp, 0, mbr) || wr(&tmp, hsect, b) || wr(&tmp, hsect + 1, fi) || wr(&tmp, hsect + 6, b) ||
        wr(&tmp, hsect + 7, fi)) {
        seterr(err, errlen, "%s", tmp.err);
        return NULL;
    }
    for (i = 2; i < res; i++) { /* rest of the reserved area */
        if (i == 6 || i == 7) continue;
        if (wr(&tmp, hsect + i, z)) {
            seterr(err, errlen, "%s", tmp.err);
            return NULL;
        }
    }
    /* FAT: first sector with media / end marks, rest zero */
    memset(b, 0, sizeof(b));
    b[0] = 0xF8;
    memset(b + 1, 0xFF, 10);
    b[11] = 0x0F;
    for (i = 0; i < fatsize; i++) {
        if (wr(&tmp, hsect + res + i, i == 0 ? b : z)) {
            seterr(err, errlen, "%s", tmp.err);
            return NULL;
        }
    }
    /* root directory cluster */
    for (i = 0; i < clust; i++) {
        if (wr(&tmp, hsect + res + fatsize + i, z)) {
            seterr(err, errlen, "%s", tmp.err);
            return NULL;
        }
    }
    return fatvol_open(io, err, errlen);
}

/* ---------------------------------------------------------------- close, error */

const char* fatvol_error(const fatvol* v) { return v ? v->err : ""; }

void fatvol_close(fatvol* v)
{
    if (!v) return;
    if (v->modified) {
        u8 fi[SECT];
        flush_fat(v);
        if (v->fsinfo && rd(v, v->fsinfo, fi) == 0) {
            p32(fi, 0x41615252u);
            p32(fi + 0x1E4, 0x61417272u);
            p32(fi + 0x1E8, v->free_cnt);
            p32(fi + 0x1EC, v->last_alloc);
            p16(fi + 510, 0xAA55);
            wr(v, v->fsinfo, fi);
            wr(v, v->hsect + 7, fi);
        }
    }
    free(v->fat);
    free(v);
}

/* ---------------------------------------------------------------- writer */

int fatvol_mkdir(fatvol* v, const char* path, fat_stamp t)
{
    u32 parent, cl;
    char name[11];
    dloc loc;
    u8 b[SECT];
    int r;
    if (v->f_active) return fail(v, "a file is being written");
    if (resolve_parent(v, path, &parent, name)) return 1;
    r = dir_find(v, parent, name, NULL, NULL);
    if (r < 0) return 1;
    if (r == 0) return fail(v, "\"%s\" already exists", path);
    if (alloc_chain(v, 1, &cl)) return 1;
    if (zero_cluster(v, cl)) goto undo;
    memset(b, 0, sizeof(b));
    memset(b, ' ', 11);
    b[0] = '.';
    b[11] = ATTR_DIR;
    p16(b + 14, t.time);
    p16(b + 16, t.date);
    p16(b + 18, t.date);
    p16(b + 20, cl >> 16);
    p16(b + 22, t.time);
    p16(b + 24, t.date);
    p16(b + 26, cl & 0xFFFF);
    memcpy(b + 32, b, 32);
    b[33] = '.';
    {
        u32 pc = (parent == v->rootcl) ? 0 : parent;
        p16(b + 32 + 20, pc >> 16);
        p16(b + 32 + 26, pc & 0xFFFF);
    }
    if (wr(v, cl_lba(v, cl), b)) goto undo;
    if (dir_slot(v, parent, &loc)) goto undo;
    if (put_entry(v, &loc, name, ATTR_DIR, t, cl, 0)) goto undo;
    return 0;
undo:
    {
        char keep[256];
        memcpy(keep, v->err, sizeof(keep));
        free_chain(v, cl);
        memcpy(v->err, keep, sizeof(keep));
    }
    return 1;
}

int fatvol_file_begin(fatvol* v, const char* path, unsigned int size, fat_stamp t)
{
    u32 parent, first = 0, cb = v->clustsz * SECT;
    char name[11];
    dloc loc;
    int r;
    if (v->f_active) return fail(v, "a file is being written");
    if (resolve_parent(v, path, &parent, name)) return 1;
    r = dir_find(v, parent, name, NULL, NULL);
    if (r < 0) return 1;
    if (r == 0) return fail(v, "\"%s\" already exists", path);
    if (size) {
        unsigned long long n = ((unsigned long long)size + cb - 1) / cb;
        if (n > v->free_cnt) return fail(v, "SD card full (%u bytes do not fit)", size);
        if (alloc_chain(v, (u32)n, &first)) return 1;
    }
    if (dir_slot(v, parent, &loc) || put_entry(v, &loc, name, ATTR_ARCH, t, first, size)) {
        char keep[256];
        memcpy(keep, v->err, sizeof(keep));
        if (first) free_chain(v, first);
        memcpy(v->err, keep, sizeof(keep));
        return 1;
    }
    v->f_active = 1;
    v->f_remaining = size;
    v->f_cl = first;
    v->f_secidx = 0;
    v->f_bufn = 0;
    return 0;
}

static int file_flush_sector(fatvol* v)
{
    if (wr(v, cl_lba(v, v->f_cl) + v->f_secidx, v->f_buf)) return 1;
    v->f_bufn = 0;
    if (++v->f_secidx == v->clustsz) {
        u32 nx = fat_get(v, v->f_cl);
        v->f_secidx = 0;
        v->f_cl = valid_cl(v, nx) ? nx : 0;
    }
    return 0;
}

int fatvol_file_write(fatvol* v, const void* data, unsigned int n)
{
    const u8* p = (const u8*)data;
    if (!v->f_active) return fail(v, "no file is open for writing");
    if (n > v->f_remaining) return fail(v, "more data than the declared size");
    v->f_remaining -= n;
    while (n) {
        u32 k = SECT - v->f_bufn;
        if (k > n) k = n;
        memcpy(v->f_buf + v->f_bufn, p, k);
        v->f_bufn += k;
        p += k;
        n -= k;
        if (v->f_bufn == SECT && file_flush_sector(v)) return 1;
    }
    return 0;
}

int fatvol_file_end(fatvol* v)
{
    if (!v->f_active) return fail(v, "no file is open for writing");
    v->f_active = 0;
    if (v->f_remaining) return fail(v, "file shorter than the declared size");
    if (v->f_cl == 0) return 0; /* empty file, or the last cluster is complete */
    if (v->f_bufn) {
        memset(v->f_buf + v->f_bufn, 0, SECT - v->f_bufn);
        if (file_flush_sector(v)) return 1;
    }
    /* zero the unused rest of the last cluster */
    if (v->f_cl && v->f_secidx) {
        u8 z[SECT];
        memset(z, 0, sizeof(z));
        while (v->f_secidx < v->clustsz) {
            if (wr(v, cl_lba(v, v->f_cl) + v->f_secidx, z)) return 1;
            v->f_secidx++;
        }
    }
    return 0;
}

int fatvol_delete(fatvol* v, const char* path)
{
    u32 parent, first, size;
    char name[11];
    dloc loc;
    u8 e[DIRSZ], b[SECT];
    int r;
    if (v->f_active) return fail(v, "a file is being written");
    if (resolve_parent(v, path, &parent, name)) return 1;
    r = dir_find(v, parent, name, &loc, e);
    if (r < 0) return 1;
    if (r > 0) return fail(v, "\"%s\" does not exist", path);
    first = g16(e + 26) | (g16(e + 20) << 16);
    size = g32(e + 28);
    (void)size;
    if (e[11] & ATTR_DIR) {
        u32 c = first, guard = v->clustnum + 2;
        int end = 0;
        while (valid_cl(v, c) && guard-- && !end) {
            u32 s, i;
            for (s = 0; s < v->clustsz && !end; s++) {
                if (rd(v, cl_lba(v, c) + s, b)) return 1;
                for (i = 0; i < SECT; i += DIRSZ) {
                    if (b[i] == 0) {
                        end = 1;
                        break;
                    }
                    if (b[i] == 0xE5) continue;
                    if (b[i] == '.' && (memcmp(b + i, ".          ", 11) == 0 || memcmp(b + i, "..         ", 11) == 0))
                        continue;
                    return fail(v, "directory \"%s\" is not empty", path);
                }
            }
            c = fat_get(v, c);
            if (c >= 0x0FFFFFF8u) break;
        }
    }
    if (rd(v, loc.sec, b)) return 1;
    b[loc.off] = 0xE5;
    if (wr(v, loc.sec, b)) return 1;
    if (first) free_chain(v, first);
    return 0;
}

/* ---------------------------------------------------------------- reader */

typedef struct {
    fatvol* v;
    int (*cb)(void*, const fatvol_entry*);
    void* ctx;
    char path[512];
} walkctx;

static int walk_dir(walkctx* w, u32 dircl, u32 plen, int depth)
{
    fatvol* v = w->v;
    u32 c = dircl, guard = v->clustnum + 2;
    u8 b[SECT];
    if (depth > MAX_DEPTH) return fail(v, "directory nesting too deep"), -1;
    while (valid_cl(v, c) && guard--) {
        u32 s, i;
        for (s = 0; s < v->clustsz; s++) {
            if (rd(v, cl_lba(v, c) + s, b)) return -1;
            for (i = 0; i < SECT; i += DIRSZ) {
                const u8* e = b + i;
                fatvol_entry fe;
                u32 n = plen, k, bl, el;
                int r;
                if (e[0] == 0) return 0;
                if (e[0] == 0xE5 || e[11] == ATTR_LFN || (e[11] & ATTR_VOL)) continue;
                if (e[0] == '.') continue;
                for (bl = 8; bl > 0 && e[bl - 1] == ' '; bl--) {}
                for (el = 3; el > 0 && e[8 + el - 1] == ' '; el--) {}
                if (n + 14 >= sizeof(w->path)) return fail(v, "path too long"), -1;
                if (n) w->path[n++] = '/';
                for (k = 0; k < bl; k++) w->path[n++] = (char)e[k];
                if (el) {
                    w->path[n++] = '.';
                    for (k = 0; k < el; k++) w->path[n++] = (char)e[8 + k];
                }
                w->path[n] = 0;
                fe.path = w->path;
                fe.is_dir = (e[11] & ATTR_DIR) != 0;
                fe.size = fe.is_dir ? 0 : g32(e + 28);
                fe.stamp.time = (unsigned short)g16(e + 22);
                fe.stamp.date = (unsigned short)g16(e + 24);
                fe.first_cluster = g16(e + 26) | (g16(e + 20) << 16);
                r = w->cb(w->ctx, &fe);
                if (r) return r;
                if (fe.is_dir && valid_cl(v, fe.first_cluster)) {
                    r = walk_dir(w, fe.first_cluster, n, depth + 1);
                    if (r) return r;
                }
                w->path[plen] = 0;
            }
        }
        c = fat_get(v, c);
        if (c >= 0x0FFFFFF8u) return 0;
    }
    return fail(v, "bad directory chain"), -1;
}

int fatvol_walk(fatvol* v, int (*cb)(void* ctx, const fatvol_entry* e), void* ctx)
{
    walkctx* w = (walkctx*)malloc(sizeof(walkctx));
    int r;
    if (!w) return fail(v, "out of memory"), -1;
    w->v = v;
    w->cb = cb;
    w->ctx = ctx;
    w->path[0] = 0;
    r = walk_dir(w, v->rootcl, 0, 0);
    free(w);
    return r;
}

int fatvol_read_file(fatvol* v, const fatvol_entry* e,
                     int (*sink)(void* ctx, const void* data, unsigned int n), void* ctx)
{
    u32 remaining = e->size, c = e->first_cluster, guard = v->clustnum + 2;
    u8 b[SECT];
    if (remaining == 0) return 0;
    while (remaining) {
        u32 s;
        if (!valid_cl(v, c) || !guard--) return fail(v, "bad cluster chain"), -1;
        for (s = 0; s < v->clustsz && remaining; s++) {
            u32 k = remaining > SECT ? SECT : remaining;
            int r;
            if (rd(v, cl_lba(v, c) + s, b)) return -1;
            r = sink(ctx, b, k);
            if (r) return r;
            remaining -= k;
        }
        if (remaining) {
            c = fat_get(v, c);
            if (c >= 0x0FFFFFF8u) return fail(v, "cluster chain shorter than file size"), -1;
        }
    }
    return 0;
}
