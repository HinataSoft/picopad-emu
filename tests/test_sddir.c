/* Test of platform/sddir: import of a host directory into a FAT image and sync back. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "../platform/fatvol.h"
#include "../platform/sddir.h"
#include "../platform/sparsedisk.h"

#define SECTORS_1G 2097152u

static int io_read(void* c, unsigned int s, unsigned char* b) { return sparse_read((sparsedisk*)c, s, b); }
static int io_write(void* c, unsigned int s, const unsigned char* b) { return sparse_write((sparsedisk*)c, s, b); }

static fatvol_io make_io(sparsedisk* d)
{
    fatvol_io io;
    io.ctx = d;
    io.sectors = sparse_sectors(d);
    io.read = io_read;
    io.write = io_write;
    return io;
}

/* ---- warnings ---- */
static char warns[4096];
static int nwarns;
static void on_warn(void* ctx, const char* msg)
{
    (void)ctx;
    nwarns++;
    if (strlen(warns) + strlen(msg) + 2 < sizeof warns) {
        strcat(warns, msg);
        strcat(warns, "\n");
    }
}
static void reset_warns(void) { warns[0] = 0; nwarns = 0; }

/* ---- host helpers ---- */
static char wd[MAX_PATH];

static void path_of(char* out, const char* rel)
{
    if (rel[0]) snprintf(out, MAX_PATH, "%s\\%s", wd, rel);
    else snprintf(out, MAX_PATH, "%s", wd);
}

static void rmtree(const char* dir)
{
    char pat[MAX_PATH], p[MAX_PATH];
    WIN32_FIND_DATAA f;
    HANDLE h;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    h = FindFirstFileA(pat, &f);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(f.cFileName, ".") || !strcmp(f.cFileName, "..")) continue;
            snprintf(p, sizeof p, "%s\\%s", dir, f.cFileName);
            if ((f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) RemoveDirectoryA(p);
            else if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rmtree(p);
            else { SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p); }
        } while (FindNextFileA(h, &f));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}

static ULONGLONG make_ft(int year, int mon, int day, int hh, int mm, int ss)
{
    SYSTEMTIME st;
    FILETIME ft;
    memset(&st, 0, sizeof st);
    st.wYear = (WORD)year; st.wMonth = (WORD)mon; st.wDay = (WORD)day;
    st.wHour = (WORD)hh; st.wMinute = (WORD)mm; st.wSecond = (WORD)ss;
    SystemTimeToFileTime(&st, &ft);
    return ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

static int set_mtime(const char* path, ULONGLONG t)
{
    FILETIME ft;
    HANDLE h = CreateFileA(path, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    ft.dwLowDateTime = (DWORD)t; ft.dwHighDateTime = (DWORD)(t >> 32);
    SetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    return 1;
}

static int get_mtime(const char* path, ULONGLONG* t)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d)) return 0;
    *t = ((ULONGLONG)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
    return 1;
}

static void mkdirs(const char* rel)
{
    char p[MAX_PATH], q[MAX_PATH];
    char* s;
    path_of(p, rel);
    for (s = p + strlen(wd) + 1; *s; s++)
        if (*s == '\\') {
            memcpy(q, p, (size_t)(s - p)); q[s - p] = 0;
            CreateDirectoryA(q, NULL);
        }
    CreateDirectoryA(p, NULL);
}

/* creates a host file (parent dirs too) with given content and mtime */
static void host_put(const char* rel, const char* content, ULONGLONG mtime)
{
    char p[MAX_PATH];
    FILE* f;
    char dir[MAX_PATH];
    char* s;
    strcpy(dir, rel);
    s = strrchr(dir, '\\');
    if (s) { *s = 0; mkdirs(dir); }
    path_of(p, rel);
    f = fopen(p, "wb");
    if (!f) { fprintf(stderr, "cannot create %s\n", p); check_failures++; return; }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    set_mtime(p, mtime);
}

/* returns malloc'd content or NULL if the file does not exist */
static char* host_get(const char* rel, long* len)
{
    char p[MAX_PATH];
    FILE* f;
    char* b;
    long n;
    path_of(p, rel);
    f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (char*)malloc((size_t)n + 1);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) n = -1;
    b[n < 0 ? 0 : n] = 0;
    fclose(f);
    if (len) *len = n;
    return b;
}

static int host_is(const char* rel, const char* expect)
{
    long n;
    char* b = host_get(rel, &n);
    int ok = b && n == (long)strlen(expect) && memcmp(b, expect, (size_t)n) == 0;
    if (!ok) fprintf(stderr, "host file %s: expected '%s', got '%s'\n", rel, expect, b ? b : "(missing)");
    free(b);
    return ok;
}

static int host_exists(const char* rel)
{
    char p[MAX_PATH];
    path_of(p, rel);
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

/* 1 if the host dir lists an entry with exactly this name (case-sensitive compare) */
static int host_lists(const char* reldir, const char* name)
{
    char pat[MAX_PATH], d[MAX_PATH];
    WIN32_FIND_DATAA f;
    HANDLE h;
    int found = 0;
    path_of(d, reldir);
    snprintf(pat, sizeof pat, "%s\\*", d);
    h = FindFirstFileA(pat, &f);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do { if (!strcmp(f.cFileName, name)) found = 1; } while (FindNextFileA(h, &f));
    FindClose(h);
    return found;
}

/* ---- image helpers ---- */
static fat_stamp mkstamp(int y, int mo, int d, int h, int mi, int s)
{
    fat_stamp t;
    t.date = (unsigned short)(((y - 1980) << 9) | (mo << 5) | d);
    t.time = (unsigned short)((h << 11) | (mi << 5) | (s / 2));
    return t;
}

static int img_put(fatvol* v, const char* path, const char* data, fat_stamp t)
{
    int r = fatvol_file_begin(v, path, (unsigned)strlen(data), t);
    if (r) return r;
    if (strlen(data) && (r = fatvol_file_write(v, data, (unsigned)strlen(data)))) return r;
    return fatvol_file_end(v);
}

typedef struct { char list[2048]; int n; } plist;
static int collect(void* ctx, const fatvol_entry* e)
{
    plist* p = (plist*)ctx;
    strcat(p->list, e->path);
    strcat(p->list, "|");
    p->n++;
    return 0;
}

/* number of entries (without . and ..) in a host directory */
static int host_count(const char* reldir)
{
    char pat[MAX_PATH], d[MAX_PATH];
    WIN32_FIND_DATAA f;
    HANDLE h;
    int n = 0;
    path_of(d, reldir);
    snprintf(pat, sizeof pat, "%s\\*", d);
    h = FindFirstFileA(pat, &f);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do { if (strcmp(f.cFileName, ".") && strcmp(f.cFileName, "..")) n++; } while (FindNextFileA(h, &f));
    FindClose(h);
    return n;
}

typedef struct { const char* path; fat_stamp st; unsigned int first; int found; } fcap;
static int find_cb(void* ctx, const fatvol_entry* e)
{
    fcap* c = (fcap*)ctx;
    if (!strcmp(e->path, c->path)) { c->st = e->stamp; c->first = e->first_cluster; c->found = 1; }
    return 0;
}

static unsigned int rdle32(const unsigned char* b)
{
    return b[0] | (b[1] << 8) | (b[2] << 16) | ((unsigned int)b[3] << 24);
}

/* geometry of the formatted volume: first FAT sector, first data sector, bytes per cluster */
static void geom(sparsedisk* d, unsigned int* fat0, unsigned int* data0, unsigned int* cb)
{
    unsigned char sec[512];
    unsigned int ps, res, nfat, fsz;
    sparse_read(d, 0, sec);
    ps = rdle32(sec + 446 + 8);
    sparse_read(d, ps, sec);
    res = sec[14] | (sec[15] << 8);
    nfat = sec[16];
    fsz = rdle32(sec + 36);
    *cb = sec[13] * 512u;
    *fat0 = ps + res;
    *data0 = ps + res + nfat * fsz;
}

/* breaks the cluster chain of a file: FAT[first cluster] = 0 */
static void break_chain(sparsedisk* d, unsigned int cluster)
{
    unsigned int fat0, data0, cb;
    unsigned char sec[512];
    geom(d, &fat0, &data0, &cb);
    sparse_read(d, fat0 + cluster / 128, sec);
    memset(sec + (cluster % 128) * 4, 0, 4);
    sparse_write(d, fat0 + cluster / 128, sec);
}

/* counting storage whose writes start failing after a limit */
typedef struct { sparsedisk* d; unsigned long count, limit; } cio;
static int cio_read(void* c, unsigned int s, unsigned char* b) { return sparse_read(((cio*)c)->d, s, b); }
static int cio_write(void* c, unsigned int s, const unsigned char* b)
{
    cio* x = (cio*)c;
    if (x->count++ >= x->limit) return 0;
    return sparse_write(x->d, s, b);
}

#define HOUR 36000000000ULL /* 100 ns units */

int main(int argc, char** argv)
{
    sparsedisk* disk;
    fatvol_io io;
    fatvol* v;
    char err[256];
    plist pl;
    ULONGLONG orig = make_ft(2020, 5, 17, 12, 0, 0);
    ULONGLONG mt_keep, mt_long, mt_a, t, t2;
    char p[MAX_PATH];
    fat_stamp now = mkstamp(2025, 6, 15, 10, 20, 30);
    int rc;

    if (argc != 2) { fprintf(stderr, "usage: test_sddir WORK_DIR\n"); return 2; }
    strncpy(wd, argv[1], sizeof wd - 1);
    rmtree(wd);
    CreateDirectoryA(wd, NULL);
    if (GetFileAttributesA(wd) == INVALID_FILE_ATTRIBUTES) {
        /* parent missing: create the chain */
        char q[MAX_PATH];
        char* s;
        strcpy(q, wd);
        for (s = q + 3; *s; s++)
            if (*s == '\\' || *s == '/') { char c = *s; *s = 0; CreateDirectoryA(q, NULL); *s = c; }
        CreateDirectoryA(wd, NULL);
    }

    /* ---- the starting tree ---- */
    host_put("DATA\\IN.TXT", "HELLO", orig);
    host_put("DATA\\MOD.TXT", "OLD", orig + 1 * HOUR);
    host_put("DATA\\DEL.TXT", "X", orig + 2 * HOUR);
    host_put("DATA\\KEEP.TXT", "KEEP", orig + 3 * HOUR);
    host_put("DATA\\HOST.TXT", "H", orig + 4 * HOUR);
    host_put("Long Name.txt", "L", orig + 5 * HOUR);
    host_put("My Docs\\A.TXT", "A", orig + 6 * HOUR);
    host_put("game\\lower.txt", "l", orig + 7 * HOUR);
    path_of(p, "DATA\\KEEP.TXT"); CHECK(get_mtime(p, &mt_keep));
    path_of(p, "Long Name.txt"); CHECK(get_mtime(p, &mt_long));
    path_of(p, "My Docs\\A.TXT"); CHECK(get_mtime(p, &mt_a));

    disk = sparse_create(SECTORS_1G);
    CHECK(disk != NULL);
    io = make_io(disk);

    /* ---- import ---- */
    reset_warns();
    err[0] = 0;
    rc = sddir_import(wd, &io, on_warn, NULL, err, sizeof err);
    CHECK_EQ(rc, 0);
    CHECK_EQ(nwarns, 2);
    CHECK(strstr(warns, "Long Name.txt") != NULL);
    CHECK(strstr(warns, "My Docs") != NULL);

    v = fatvol_open(&io, err, sizeof err);
    CHECK(v != NULL);
    if (!v) { fprintf(stderr, "open: %s\n", err); return 1; }
    memset(&pl, 0, sizeof pl);
    CHECK_EQ(fatvol_walk(v, collect, &pl), 0);
    CHECK_EQ(pl.n, 8);
    CHECK(strstr(pl.list, "DATA/IN.TXT|") != NULL);
    CHECK(strstr(pl.list, "GAME/LOWER.TXT|") != NULL);
    CHECK(strstr(pl.list, "LONG") == NULL);
    CHECK(strstr(pl.list, "MY") == NULL);
    CHECK(strstr(pl.list, "A.TXT") == NULL);

    /* ---- the game ---- */
    CHECK_EQ(img_put(v, "DATA/OUT.TXT", "OK", now), 0);
    CHECK_EQ(fatvol_mkdir(v, "NEWDIR", now), 0);
    CHECK_EQ(img_put(v, "NEWDIR/NEW.TXT", "NEW", now), 0);
    CHECK_EQ(fatvol_delete(v, "DATA/MOD.TXT"), 0);
    CHECK_EQ(img_put(v, "DATA/MOD.TXT", "CHANGED", now), 0);
    CHECK_EQ(fatvol_delete(v, "DATA/DEL.TXT"), 0);
    CHECK_EQ(fatvol_delete(v, "DATA/HOST.TXT"), 0);
    CHECK_EQ(img_put(v, "DATA/HOST.TXT", "GAME", now), 0);
    fatvol_close(v);

    /* the user changes HOST.TXT meanwhile */
    host_put("DATA\\HOST.TXT", "USER", make_ft(2026, 1, 2, 3, 4, 6));

    /* ---- sync ---- */
    reset_warns();
    rc = sddir_sync(&io, on_warn, NULL);
    CHECK_EQ(rc, 1);
    CHECK_EQ(nwarns, 1);
    CHECK(strstr(warns, "DATA/HOST.TXT") != NULL);
    CHECK(host_is("DATA\\OUT.TXT", "OK"));
    CHECK(host_is("NEWDIR\\NEW.TXT", "NEW"));
    CHECK(host_is("DATA\\MOD.TXT", "CHANGED"));
    CHECK(!host_exists("DATA\\DEL.TXT"));
    CHECK(host_is("DATA\\KEEP.TXT", "KEEP"));
    CHECK(host_is("DATA\\IN.TXT", "HELLO"));
    CHECK(host_is("DATA\\HOST.TXT", "USER"));
    CHECK(host_is("Long Name.txt", "L"));
    CHECK(host_is("My Docs\\A.TXT", "A"));
    CHECK(host_is("game\\lower.txt", "l"));
    CHECK(host_lists("game", "lower.txt"));
    CHECK(host_lists("", "game"));
    CHECK(!host_lists("game", "LOWER.TXT"));
    path_of(p, "DATA\\KEEP.TXT"); CHECK(get_mtime(p, &t)); CHECK(t == mt_keep);
    path_of(p, "Long Name.txt"); CHECK(get_mtime(p, &t)); CHECK(t == mt_long);
    path_of(p, "My Docs\\A.TXT"); CHECK(get_mtime(p, &t)); CHECK(t == mt_a);
    path_of(p, "DATA\\IN.TXT"); CHECK(get_mtime(p, &t)); CHECK(t == orig);

    /* mtime of a written file follows the FAT stamp */
    {
        FILETIME l, u;
        DosDateTimeToFileTime(now.date, now.time, &l);
        LocalFileTimeToFileTime(&l, &u);
        t2 = ((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime;
        path_of(p, "DATA\\OUT.TXT");
        CHECK(get_mtime(p, &t));
        CHECK((t > t2 ? t - t2 : t2 - t) <= 20000000ULL);
    }

    /* second sync is a no-op */
    host_put("DATA\\OUT.TXT", "TOUCHED", make_ft(2026, 2, 2, 2, 2, 2));
    reset_warns();
    CHECK_EQ(sddir_sync(&io, on_warn, NULL), 0);
    CHECK_EQ(nwarns, 0);
    CHECK(host_is("DATA\\OUT.TXT", "TOUCHED"));

    /* ---- import of a missing dir / of a file ---- */
    reset_warns();
    err[0] = 0;
    {
        char nope[MAX_PATH];
        path_of(nope, "no-such-dir");
        CHECK_EQ(sddir_import(nope, &io, on_warn, NULL, err, sizeof err), -1);
        CHECK(err[0] != 0);
        path_of(nope, "DATA\\IN.TXT");
        err[0] = 0;
        CHECK_EQ(sddir_import(nope, &io, on_warn, NULL, err, sizeof err), -1);
        CHECK(err[0] != 0);
    }

    /* ---- scenario 2: deletions of directories, conflicts on delete, a path that appeared ---- */
    sparse_free(disk);
    disk = sparse_create(SECTORS_1G);
    io = make_io(disk);
    host_put("GONE\\X.TXT", "x", orig);
    host_put("GONE\\long name.txt", "keepme", orig);
    mkdirs("EMPT");
    host_put("F.TXT", "f", orig);
    host_put("G.TXT", "g", orig);
    reset_warns();
    CHECK_EQ(sddir_import(wd, &io, on_warn, NULL, err, sizeof err), 0);
    v = fatvol_open(&io, err, sizeof err);
    CHECK(v != NULL);
    if (!v) return 1;
    CHECK_EQ(fatvol_delete(v, "GONE/X.TXT"), 0);
    CHECK_EQ(fatvol_delete(v, "GONE"), 0);
    CHECK_EQ(fatvol_delete(v, "EMPT"), 0);
    CHECK_EQ(fatvol_delete(v, "F.TXT"), 0);
    CHECK_EQ(fatvol_delete(v, "G.TXT"), 0);
    CHECK_EQ(img_put(v, "NEWF.TXT", "GAMENEW", now), 0);
    fatvol_close(v);
    host_put("F.TXT", "user-f", make_ft(2026, 3, 3, 3, 3, 4));
    host_put("NEWF.TXT", "usernew", make_ft(2026, 3, 3, 3, 3, 4));
    reset_warns();
    CHECK_EQ(sddir_sync(&io, on_warn, NULL), 3);
    CHECK(strstr(warns, "GONE") != NULL);
    CHECK(strstr(warns, "F.TXT") != NULL);
    CHECK(strstr(warns, "NEWF.TXT") != NULL);
    CHECK(!host_exists("GONE\\X.TXT"));
    CHECK(host_is("GONE\\long name.txt", "keepme"));
    CHECK(!host_exists("EMPT"));
    CHECK(host_is("F.TXT", "user-f"));
    CHECK(!host_exists("G.TXT"));
    CHECK(host_is("NEWF.TXT", "usernew"));

    /* ---- scenario 3: a damaged image file never truncates or leaves a temp on the host; hash-only change ---- */
    sparse_free(disk);
    rmtree(wd);
    CreateDirectoryA(wd, NULL);
    disk = sparse_create(SECTORS_1G);
    io = make_io(disk);
    host_put("ATOM\\A.TXT", "AAAA", orig);
    host_put("HASH\\H.TXT", "aaaa", orig + HOUR);
    path_of(p, "ATOM\\A.TXT"); CHECK(get_mtime(p, &mt_a));
    reset_warns();
    CHECK_EQ(sddir_import(wd, &io, on_warn, NULL, err, sizeof err), 0);
    v = fatvol_open(&io, err, sizeof err);
    CHECK(v != NULL);
    if (!v) return 1;
    {
        unsigned int fat0, data0, cb;
        char* big;
        fcap fc;
        geom(disk, &fat0, &data0, &cb);
        big = (char*)malloc(3 * cb + 2);
        memset(big, 'Z', 3 * cb + 1);
        big[3 * cb + 1] = 0;
        CHECK_EQ(fatvol_delete(v, "ATOM/A.TXT"), 0);
        CHECK_EQ(img_put(v, "ATOM/A.TXT", big, now), 0);
        CHECK_EQ(img_put(v, "ATOM/N.TXT", big, now), 0);
        /* hash-only change: same size, same stamp, other content */
        memset(&fc, 0, sizeof fc);
        fc.path = "HASH/H.TXT";
        CHECK_EQ(fatvol_walk(v, find_cb, &fc), 0);
        CHECK(fc.found);
        CHECK_EQ(fatvol_delete(v, "HASH/H.TXT"), 0);
        CHECK_EQ(img_put(v, "HASH/H.TXT", "bbbb", fc.st), 0);
        CHECK_EQ(fatvol_close(v), 0);
        v = fatvol_open(&io, err, sizeof err);
        CHECK(v != NULL);
        if (!v) return 1;
        memset(&fc, 0, sizeof fc);
        fc.path = "ATOM/A.TXT";
        CHECK_EQ(fatvol_walk(v, find_cb, &fc), 0);
        CHECK(fc.found);
        break_chain(disk, fc.first);
        memset(&fc, 0, sizeof fc);
        fc.path = "ATOM/N.TXT";
        CHECK_EQ(fatvol_walk(v, find_cb, &fc), 0);
        CHECK(fc.found);
        break_chain(disk, fc.first);
        fatvol_close(v);
        free(big);
    }
    reset_warns();
    CHECK_EQ(sddir_sync(&io, on_warn, NULL), 2);
    CHECK(strstr(warns, "ATOM/A.TXT") != NULL);
    CHECK(strstr(warns, "ATOM/N.TXT") != NULL);
    CHECK(host_is("ATOM\\A.TXT", "AAAA"));
    path_of(p, "ATOM\\A.TXT"); CHECK(get_mtime(p, &t)); CHECK(t == mt_a);
    CHECK(!host_exists("ATOM\\N.TXT"));
    CHECK_EQ(host_count("ATOM"), 1); /* no temp files left */
    CHECK(host_is("HASH\\H.TXT", "bbbb")); /* hash-only change reaches the host */
    CHECK_EQ(host_count("HASH"), 1);

    /* ---- scenario 4: names the import refuses, reparse points ---- */
    sparse_free(disk);
    rmtree(wd);
    CreateDirectoryA(wd, NULL);
    disk = sparse_create(SECTORS_1G);
    io = make_io(disk);
    {
        char cmd[2 * MAX_PATH + 40], link[MAX_PATH], outside[MAX_PATH], q[MAX_PATH];
        /* the junction target lives outside of the synced directory */
        snprintf(outside, sizeof outside, "%s_outside", wd);
        rmtree(outside);
        CreateDirectoryA(outside, NULL);
        path_of(link, "JUNC");
        snprintf(cmd, sizeof cmd, "mklink /J \"%s\" \"%s\" >nul", link, outside);
        CHECK_EQ(system(cmd), 0);
        reset_warns();
        CHECK_EQ(sddir_import(wd, &io, on_warn, NULL, err, sizeof err), 0);
        CHECK(strstr(warns, "JUNC") != NULL);
        v = fatvol_open(&io, err, sizeof err);
        CHECK(v != NULL);
        if (!v) return 1;
        CHECK_EQ(fatvol_mkdir(v, "JUNC", now), 0);
        CHECK_EQ(img_put(v, "JUNC/IN.TXT", "THRU", now), 0);
        /* a name lib_fat accepts but the import refuses: patch AXB.TXT to "A B.TXT" in the root */
        CHECK_EQ(img_put(v, "AXB.TXT", "SP", now), 0);
        CHECK_EQ(fatvol_close(v), 0);
        {
            unsigned int fat0, data0, cb, i, j;
            unsigned char sec[512];
            int patched = 0;
            geom(disk, &fat0, &data0, &cb);
            for (i = 0; i < cb / 512 && !patched; i++) {
                sparse_read(disk, data0 + i, sec);
                for (j = 0; j < 512; j += 32)
                    if (!memcmp(sec + j, "AXB     TXT", 11)) { sec[j + 1] = ' '; patched = 1; }
                if (patched) sparse_write(disk, data0 + i, sec);
            }
            CHECK(patched);
        }
        reset_warns();
        sddir_sync(&io, on_warn, NULL);
        CHECK(strstr(warns, "JUNC") != NULL);
        CHECK(strstr(warns, "A B.TXT") != NULL);
        CHECK(strstr(warns, "not be loaded") != NULL);
        CHECK(host_is("A B.TXT", "SP"));
        snprintf(q, sizeof q, "%s\\IN.TXT", outside);
        CHECK(GetFileAttributesA(q) == INVALID_FILE_ATTRIBUTES); /* nothing written through the junction */
        RemoveDirectoryA(link);
        rmtree(outside);
    }

    /* ---- scenario 5: a failing flush at the end of the import fails the import ---- */
    sparse_free(disk);
    rmtree(wd);
    CreateDirectoryA(wd, NULL);
    host_put("X.TXT", "x", orig);
    {
        cio c;
        fatvol_io cio_io;
        unsigned long total;
        c.d = sparse_create(SECTORS_1G);
        c.count = 0;
        c.limit = 0xFFFFFFFFul;
        cio_io.ctx = &c;
        cio_io.sectors = SECTORS_1G;
        cio_io.read = cio_read;
        cio_io.write = cio_write;
        CHECK_EQ(sddir_import(wd, &cio_io, on_warn, NULL, err, sizeof err), 0);
        total = c.count;
        sparse_free(c.d);
        c.d = sparse_create(SECTORS_1G);
        c.count = 0;
        c.limit = total - 1; /* the last FSInfo write fails */
        err[0] = 0;
        CHECK_EQ(sddir_import(wd, &cio_io, on_warn, NULL, err, sizeof err), -1);
        CHECK(strstr(err, "out of memory") != NULL);
        sparse_free(c.d);
    }
    return CHECK_DONE();
}
