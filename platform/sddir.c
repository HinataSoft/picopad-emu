/* --sddir: import of a host directory into a FAT32 image and sync back. See sddir.h.

   Manifest: one entry per imported file or directory, in import (pre-order) order:
   FAT path, host path, is_dir, host size + mtime at import (compared with host values
   only), FAT size + stamp + FNV-1a content hash at import (compared with FAT values
   only). Names that are not valid 8.3 names are not imported and are never in the
   manifest, so the sync never touches them.

   Sync order: (1) walk the image once; (2) every image entry in pre-order: in the
   manifest -> changed? (size, stamp or hash differs from import) -> write unless the host
   copy changed since import; not in the manifest -> new: create below the host parent
   unless the path exists on the host; (3) manifest entries not seen in the image, in
   reverse order (children first): delete unless the host copy changed; a directory only
   if empty. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "sddir.h"

typedef unsigned long long u64;

/* ---------------------------------------------------------------- warnings */
typedef struct {
    sddir_warn_fn fn;
    void* ctx;
    int count;
} wsink;

static void warnf(wsink* w, const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    w->count++;
    if (w->fn) w->fn(w->ctx, buf);
}

/* ------------------------------------------------------------ strings, utf */
static char* w2u(const wchar_t* w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char* s = (char*)malloc((size_t)(n > 0 ? n : 1));
    if (!s) return NULL;
    if (n <= 0 || !WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL)) s[0] = 0;
    return s;
}

static wchar_t* u2w(const char* s)
{
    UINT cp = CP_UTF8;
    DWORD fl = MB_ERR_INVALID_CHARS;
    int n = MultiByteToWideChar(cp, fl, s, -1, NULL, 0);
    wchar_t* w;
    if (n <= 0) { cp = CP_ACP; fl = 0; n = MultiByteToWideChar(cp, fl, s, -1, NULL, 0); }
    if (n <= 0) return NULL;
    w = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)n);
    if (!w) return NULL;
    if (!MultiByteToWideChar(cp, fl, s, -1, w, n)) { free(w); return NULL; }
    return w;
}

static wchar_t* wjoin(const wchar_t* a, const wchar_t* b)
{
    size_t la = wcslen(a), lb = wcslen(b);
    int sep = la > 0 && a[la - 1] != L'\\' && a[la - 1] != L'/';
    wchar_t* r = (wchar_t*)malloc(sizeof(wchar_t) * (la + lb + 2));
    if (!r) return NULL;
    memcpy(r, a, sizeof(wchar_t) * la);
    if (sep) r[la++] = L'\\';
    memcpy(r + la, b, sizeof(wchar_t) * (lb + 1));
    return r;
}

static wchar_t* ascii2w(const char* s)
{
    size_t n = strlen(s), i;
    wchar_t* w = (wchar_t*)malloc(sizeof(wchar_t) * (n + 1));
    if (!w) return NULL;
    for (i = 0; i <= n; i++) w[i] = (unsigned char)s[i];
    return w;
}

static char* xstrdup(const char* s)
{
    size_t n = strlen(s) + 1;
    char* r = (char*)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

/* ---------------------------------------------------------------- 8.3 rule */
/* identical to fo_check_name() in tools/sdimg/fat_ops.c (one segment); out = uppercased name */
static int valid_char(unsigned int c)
{
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= '0' && c <= '9') return 1;
    return c != 0 && c < 128 && strchr("!#$%&'()-@^_`{}~", (int)c) != NULL;
}

static int fat_name(const wchar_t* w, char* out)
{
    int base = 0, ext = 0, dot = 0, n = 0;
    for (; *w; w++) {
        unsigned int c = *w;
        if (c == '.') {
            if (dot || base == 0) return 0;
            dot = 1;
        } else if (!valid_char(c) || (dot ? ++ext > 3 : ++base > 8)) {
            return 0;
        }
        out[n++] = (char)((c >= 'a' && c <= 'z') ? c - 32 : c);
    }
    if (dot && ext == 0) return 0;
    if (n == 0) return 0;
    out[n] = 0;
    return 1;
}

/* ------------------------------------------------------------------- times */
static u64 ft64(const FILETIME* f) { return ((u64)f->dwHighDateTime << 32) | f->dwLowDateTime; }

static fat_stamp ft_to_stamp(u64 m)
{
    FILETIME f, l;
    WORD d = 0, t = 0;
    fat_stamp s;
    f.dwLowDateTime = (DWORD)m;
    f.dwHighDateTime = (DWORD)(m >> 32);
    if (!FileTimeToLocalFileTime(&f, &l) || !FileTimeToDosDateTime(&l, &d, &t)) {
        d = (1 << 5) | 1; /* 1980-01-01 00:00:00 */
        t = 0;
    }
    s.date = d;
    s.time = t;
    return s;
}

static FILETIME stamp_to_ft(fat_stamp s)
{
    FILETIME l, u;
    if (!DosDateTimeToFileTime(s.date, s.time, &l)) DosDateTimeToFileTime((1 << 5) | 1, 0, &l);
    if (!LocalFileTimeToFileTime(&l, &u)) u = l;
    return u;
}

/* -------------------------------------------------------------------- host */
static int hstat(const wchar_t* p, int* isdir, u64* size, u64* mtime)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &d)) return 0;
    if (isdir) *isdir = (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (size) *size = ((u64)d.nFileSizeHigh << 32) | d.nFileSizeLow;
    if (mtime) *mtime = ft64(&d.ftLastWriteTime);
    return 1;
}

static void mkdir_p(const wchar_t* path)
{
    wchar_t* q = (wchar_t*)malloc(sizeof(wchar_t) * (wcslen(path) + 1));
    wchar_t* s;
    if (!q) return;
    wcscpy(q, path);
    for (s = q + 1; *s; s++)
        if (*s == L'\\' || *s == L'/') {
            wchar_t c = *s;
            *s = 0;
            CreateDirectoryW(q, NULL);
            *s = c;
        }
    CreateDirectoryW(q, NULL);
    free(q);
}

/* ------------------------------------------------------------ string map */
typedef struct {
    char** k;
    int* v;
    size_t cap, n;
} smap;

static u64 fnv_str(const char* s)
{
    u64 h = 1469598103934665603ULL;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 1099511628211ULL;
    return h;
}

static int smap_init(smap* m)
{
    m->cap = 1024;
    m->n = 0;
    m->k = (char**)calloc(m->cap, sizeof(char*));
    m->v = (int*)calloc(m->cap, sizeof(int));
    return m->k && m->v;
}

static void smap_free(smap* m)
{
    free(m->k);
    free(m->v);
    m->k = NULL;
    m->v = NULL;
}

static int smap_put(smap* m, char* key, int val);

static int smap_grow(smap* m)
{
    smap nm;
    size_t i;
    nm.cap = m->cap * 2;
    nm.n = 0;
    nm.k = (char**)calloc(nm.cap, sizeof(char*));
    nm.v = (int*)calloc(nm.cap, sizeof(int));
    if (!nm.k || !nm.v) { free(nm.k); free(nm.v); return 0; }
    for (i = 0; i < m->cap; i++)
        if (m->k[i]) smap_put(&nm, m->k[i], m->v[i]);
    smap_free(m);
    *m = nm;
    return 1;
}

static int smap_put(smap* m, char* key, int val)
{
    size_t i;
    if ((m->n + 1) * 2 > m->cap && !smap_grow(m)) return 0;
    i = (size_t)(fnv_str(key) % m->cap);
    while (m->k[i]) {
        if (!strcmp(m->k[i], key)) { m->v[i] = val; return 1; }
        i = (i + 1) % m->cap;
    }
    m->k[i] = key;
    m->v[i] = val;
    m->n++;
    return 1;
}

static int smap_get(const smap* m, const char* key)
{
    size_t i = (size_t)(fnv_str(key) % m->cap);
    while (m->k[i]) {
        if (!strcmp(m->k[i], key)) return m->v[i];
        i = (i + 1) % m->cap;
    }
    return -1;
}

/* ---------------------------------------------------------------- manifest */
typedef struct {
    char* fat;      /* FAT path; "" never stored */
    wchar_t* host;  /* host path; NULL = blocked (a new directory that could not be created) */
    int is_dir;
    int seen;
    u64 hsize, hmtime; /* host values at import */
    unsigned int fsize;
    fat_stamp fstamp;  /* FAT values at import */
    u64 hash;
} mentry;

typedef struct {
    mentry* e;
    size_t n, cap;
    wchar_t* root;
} manifest;

static manifest g_m;
static int g_have;

static void m_free(manifest* m)
{
    size_t i;
    for (i = 0; i < m->n; i++) { free(m->e[i].fat); free(m->e[i].host); }
    free(m->e);
    free(m->root);
    memset(m, 0, sizeof *m);
}

static mentry* m_add(manifest* m)
{
    if (m->n == m->cap) {
        size_t nc = m->cap ? m->cap * 2 : 256;
        mentry* ne = (mentry*)realloc(m->e, nc * sizeof(mentry));
        if (!ne) return NULL;
        m->e = ne;
        m->cap = nc;
    }
    memset(&m->e[m->n], 0, sizeof(mentry));
    return &m->e[m->n++];
}

static u64 fnv_update(u64 h, const unsigned char* p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) h = (h ^ p[i]) * 1099511628211ULL;
    return h;
}
#define FNV_INIT 1469598103934665603ULL

/* ------------------------------------------------------------------ import */
typedef struct {
    fatvol* v;
    wsink* w;
    char* err;
    int errlen;
} imp;

static void set_err(imp* I, const char* rel, const char* msg)
{
    if (I->errlen > 0) snprintf(I->err, (size_t)I->errlen, "%s: %s", rel, msg);
}

static int imp_file(imp* I, const wchar_t* hp, const char* fpath, const char* relu, u64 size, u64 mtime,
                    mentry* me)
{
    static unsigned char buf[65536];
    HANDLE h;
    u64 left = size, hash = FNV_INIT;
    int short_read = 0;
    fat_stamp st = ft_to_stamp(mtime);
    if (size > 0xFFFFFFFFULL) { set_err(I, relu, "file too large for the SD card"); return -1; }
    h = CreateFileW(hp, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { set_err(I, relu, "cannot read the file"); return -1; }
    if (fatvol_file_begin(I->v, fpath, (unsigned int)size, st)) {
        set_err(I, relu, fatvol_error(I->v));
        CloseHandle(h);
        return -1;
    }
    while (left > 0) {
        DWORD want = left > sizeof buf ? (DWORD)sizeof buf : (DWORD)left, got = 0;
        if (!ReadFile(h, buf, want, &got, NULL)) {
            set_err(I, relu, "read error");
            CloseHandle(h);
            return -1;
        }
        if (got == 0) { /* file shrank while being read: pad with zeros */
            memset(buf, 0, want);
            got = want;
            short_read = 1;
        }
        hash = fnv_update(hash, buf, got);
        if (fatvol_file_write(I->v, buf, got)) {
            set_err(I, relu, fatvol_error(I->v));
            CloseHandle(h);
            return -1;
        }
        left -= got;
    }
    CloseHandle(h);
    if (fatvol_file_end(I->v)) { set_err(I, relu, fatvol_error(I->v)); return -1; }
    if (short_read) warnf(I->w, "%s: changed while it was being loaded", relu);
    me->hash = hash;
    return 0;
}

static int imp_dir(imp* I, const wchar_t* hdir, const wchar_t* rel, const char* fpath)
{
    wchar_t* pat = wjoin(hdir, L"*");
    WIN32_FIND_DATAW f;
    HANDLE h;
    int rc = 0;
    if (!pat) { set_err(I, "", "out of memory"); return -1; }
    h = FindFirstFileW(pat, &f);
    free(pat);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        wchar_t* hp = NULL;
        wchar_t* rl = NULL;
        char* relu = NULL;
        char fname[16];
        char* fp = NULL;
        int is_dir = (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!wcscmp(f.cFileName, L".") || !wcscmp(f.cFileName, L"..")) continue;
        rl = rel[0] ? wjoin(rel, f.cFileName) : wjoin(L"", f.cFileName);
        relu = rl ? w2u(rl) : NULL;
        hp = wjoin(hdir, f.cFileName);
        if (!rl || !relu || !hp) { set_err(I, "", "out of memory"); rc = -1; goto next; }
        if (!fat_name(f.cFileName, fname)) {
            warnf(I->w, "not loaded (not a valid 8.3 name; the sync leaves it alone): %s", relu);
            goto next;
        }
        if (is_dir && (f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
            warnf(I->w, "not loaded (link to a directory; the sync leaves it alone): %s", relu);
            goto next;
        }
        {
            size_t fl = strlen(fpath) + strlen(fname) + 2;
            mentry* me;
            u64 mt = ft64(&f.ftLastWriteTime);
            u64 sz = ((u64)f.nFileSizeHigh << 32) | f.nFileSizeLow;
            fp = (char*)malloc(fl);
            if (!fp) { set_err(I, "", "out of memory"); rc = -1; goto next; }
            snprintf(fp, fl, "%s%s%s", fpath, fpath[0] ? "/" : "", fname);
            me = m_add(&g_m);
            if (!me) { set_err(I, "", "out of memory"); rc = -1; goto next; }
            me->fat = fp;
            fp = NULL;
            me->host = hp;
            hp = NULL;
            me->is_dir = is_dir;
            me->hmtime = mt;
            me->hsize = is_dir ? 0 : sz;
            me->fstamp = ft_to_stamp(mt);
            if (is_dir) {
                if (fatvol_mkdir(I->v, me->fat, me->fstamp)) { set_err(I, relu, fatvol_error(I->v)); rc = -1; goto next; }
                /* g_m.e may move while recursing: pass copies */
                {
                    wchar_t* hcopy = (wchar_t*)malloc(sizeof(wchar_t) * (wcslen(me->host) + 1));
                    char* fcopy = xstrdup(me->fat);
                    if (!hcopy || !fcopy) { free(hcopy); free(fcopy); set_err(I, "", "out of memory"); rc = -1; goto next; }
                    wcscpy(hcopy, me->host);
                    rc = imp_dir(I, hcopy, rl, fcopy);
                    free(hcopy);
                    free(fcopy);
                }
            } else {
                size_t idx = (size_t)(me - g_m.e);
                mentry tmp = *me;
                rc = imp_file(I, me->host, me->fat, relu, sz, mt, &tmp);
                g_m.e[idx].hash = tmp.hash;
                me = &g_m.e[idx];
                me->fsize = (unsigned int)sz;
            }
        }
    next:
        free(hp);
        free(rl);
        free(relu);
        free(fp);
        if (rc) break;
    } while (FindNextFileW(h, &f));
    FindClose(h);
    return rc;
}

int sddir_import(const char* dir, const fatvol_io* io, sddir_warn_fn warn, void* wctx, char* err, int errlen)
{
    wsink w;
    imp I;
    wchar_t* wd;
    DWORD attr;
    fatvol* v;
    int rc;
    if (err && errlen > 0) err[0] = 0;
    if (g_have) { m_free(&g_m); g_have = 0; }
    w.fn = warn; w.ctx = wctx; w.count = 0;
    wd = u2w(dir);
    if (!wd) { if (err) snprintf(err, (size_t)errlen, "%s: invalid path", dir); return -1; }
    attr = GetFileAttributesW(wd);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        if (err) snprintf(err, (size_t)errlen, "%s: no such directory", dir);
        free(wd);
        return -1;
    }
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        if (err) snprintf(err, (size_t)errlen, "%s: not a directory", dir);
        free(wd);
        return -1;
    }
    v = fatvol_format(io, err, errlen);
    if (!v) { free(wd); return -1; }
    memset(&g_m, 0, sizeof g_m);
    g_m.root = wd;
    I.v = v; I.w = &w; I.err = err; I.errlen = errlen;
    rc = imp_dir(&I, wd, L"", "");
    fatvol_close(v);
    if (rc) { m_free(&g_m); return -1; }
    g_have = 1;
    return 0;
}

/* -------------------------------------------------------------------- sync */
typedef struct {
    char* path;
    int is_dir;
    unsigned int size, first_cluster;
    fat_stamp stamp;
} ient;

typedef struct {
    ient* a;
    size_t n, cap;
    int oom;
} ilist;

static int collect_cb(void* ctx, const fatvol_entry* e)
{
    ilist* L = (ilist*)ctx;
    ient* x;
    if (L->n == L->cap) {
        size_t nc = L->cap ? L->cap * 2 : 256;
        ient* na = (ient*)realloc(L->a, nc * sizeof(ient));
        if (!na) { L->oom = 1; return 1; }
        L->a = na;
        L->cap = nc;
    }
    x = &L->a[L->n];
    x->path = xstrdup(e->path);
    if (!x->path) { L->oom = 1; return 1; }
    x->is_dir = e->is_dir;
    x->size = e->size;
    x->first_cluster = e->first_cluster;
    x->stamp = e->stamp;
    L->n++;
    return 0;
}

static void ilist_free(ilist* L)
{
    size_t i;
    for (i = 0; i < L->n; i++) free(L->a[i].path);
    free(L->a);
}

static fatvol_entry to_entry(const ient* x)
{
    fatvol_entry e;
    e.path = x->path;
    e.is_dir = x->is_dir;
    e.size = x->size;
    e.stamp = x->stamp;
    e.first_cluster = x->first_cluster;
    return e;
}

static int hash_sink(void* ctx, const void* data, unsigned int n)
{
    u64* h = (u64*)ctx;
    *h = fnv_update(*h, (const unsigned char*)data, n);
    return 0;
}

typedef struct {
    HANDLE h;
    int bad;
} wsinkf;

static int file_sink(void* ctx, const void* data, unsigned int n)
{
    wsinkf* s = (wsinkf*)ctx;
    DWORD put = 0;
    if (!WriteFile(s->h, data, n, &put, NULL) || put != n) { s->bad = 1; return 1; }
    return 0;
}

/* writes the content of an image file to a host path; 0 ok */
static int write_host(fatvol* v, const ient* x, const wchar_t* hp, int overwrite, wsink* w)
{
    wsinkf s;
    fatvol_entry e = to_entry(x);
    FILETIME ft = stamp_to_ft(x->stamp);
    int rc;
    s.bad = 0;
    s.h = CreateFileW(hp, GENERIC_WRITE, 0, NULL, overwrite ? CREATE_ALWAYS : CREATE_NEW,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (s.h == INVALID_HANDLE_VALUE) {
        warnf(w, "%s: cannot write to the directory (error %lu)", x->path, (unsigned long)GetLastError());
        return -1;
    }
    rc = fatvol_read_file(v, &e, file_sink, &s);
    if (rc || s.bad) {
        warnf(w, "%s: %s", x->path, s.bad ? "write to the directory failed" : "damaged in the image");
        CloseHandle(s.h);
        if (!overwrite) DeleteFileW(hp);
        return -1;
    }
    SetFileTime(s.h, NULL, NULL, &ft);
    CloseHandle(s.h);
    return 0;
}

static void parent_of(const char* path, char* out)
{
    const char* s = strrchr(path, '/');
    size_t n = s ? (size_t)(s - path) : 0;
    memcpy(out, path, n);
    out[n] = 0;
}

static void sync_core(fatvol* v, manifest* m, wsink* w)
{
    ilist L;
    smap map;
    size_t n0 = m->n, i;
    int rc;
    memset(&L, 0, sizeof L);
    rc = fatvol_walk(v, collect_cb, &L);
    if (rc || L.oom) {
        warnf(w, "the SD image could not be read (%s); the directory was left as it is",
              L.oom ? "out of memory" : fatvol_error(v));
        ilist_free(&L);
        return;
    }
    if (!smap_init(&map)) { warnf(w, "out of memory"); ilist_free(&L); return; }
    for (i = 0; i < n0; i++) smap_put(&map, m->e[i].fat, (int)i);

    for (i = 0; i < L.n; i++) {
        ient* x = &L.a[i];
        int idx = smap_get(&map, x->path);
        if (idx >= 0 && (size_t)idx < n0) { /* loaded at start */
            mentry* me = &m->e[idx];
            int changed;
            int isdir;
            u64 hs, hm;
            me->seen = 1;
            if (me->is_dir != x->is_dir) {
                warnf(w, "%s: changed between file and directory; left as it is", x->path);
                continue;
            }
            if (x->is_dir) continue;
            changed = x->size != me->fsize || x->stamp.date != me->fstamp.date || x->stamp.time != me->fstamp.time;
            if (!changed) {
                u64 h = FNV_INIT;
                fatvol_entry e = to_entry(x);
                if (fatvol_read_file(v, &e, hash_sink, &h)) {
                    warnf(w, "%s: damaged in the image; left as it is", x->path);
                    continue;
                }
                changed = h != me->hash;
            }
            if (!changed) continue;
            if (!hstat(me->host, &isdir, &hs, &hm) || isdir || hs != me->hsize || hm != me->hmtime) {
                warnf(w, "%s: changed on the host since start, the game's version was discarded", x->path);
                continue;
            }
            write_host(v, x, me->host, 1, w);
        } else if (idx < 0) { /* new */
            char par[512];
            wchar_t* name = NULL;
            wchar_t* hp = NULL;
            const wchar_t* phost;
            int pidx, exists, isdir;
            size_t plen = strlen(x->path);
            if (plen >= sizeof par) { warnf(w, "%s: path too long; skipped", x->path); continue; }
            parent_of(x->path, par);
            if (par[0]) {
                pidx = smap_get(&map, par);
                if (pidx < 0 || !m->e[pidx].host) continue; /* parent missing or blocked (warned) */
                phost = m->e[pidx].host;
            } else {
                phost = m->root;
            }
            name = ascii2w(strrchr(x->path, '/') ? strrchr(x->path, '/') + 1 : x->path);
            hp = name ? wjoin(phost, name) : NULL;
            free(name);
            if (!hp) { warnf(w, "out of memory"); continue; }
            exists = hstat(hp, &isdir, NULL, NULL);
            if (x->is_dir) {
                mentry* ne;
                char* key;
                if (!exists && !CreateDirectoryW(hp, NULL)) {
                    warnf(w, "%s: cannot create the directory (error %lu)", x->path, (unsigned long)GetLastError());
                    free(hp);
                    hp = NULL; /* blocked */
                } else if (exists && !isdir) {
                    warnf(w, "%s: a file of that name appeared on the host; left as it is", x->path);
                    free(hp);
                    hp = NULL;
                }
                ne = m_add(m);
                key = xstrdup(x->path);
                if (!ne || !key) { warnf(w, "out of memory"); free(hp); free(key); continue; }
                ne->fat = key;
                ne->host = hp;
                ne->is_dir = 1;
                ne->seen = 1;
                smap_put(&map, ne->fat, (int)(ne - m->e));
            } else {
                if (exists) {
                    warnf(w, "%s: appeared on the host meanwhile; left as it is", x->path);
                } else {
                    write_host(v, x, hp, 0, w);
                }
                free(hp);
            }
        }
    }

    /* deletions: loaded at start and gone from the image, children before parents */
    for (i = n0; i-- > 0;) {
        mentry* me = &m->e[i];
        int isdir;
        u64 hs, hm;
        if (me->seen) continue;
        if (!hstat(me->host, &isdir, &hs, &hm)) continue; /* already gone */
        if (me->is_dir) {
            if (!isdir || !RemoveDirectoryW(me->host))
                warnf(w, "%s: deleted by the game but not removed from the directory (not empty)", me->fat);
        } else {
            if (isdir || hs != me->hsize || hm != me->hmtime) {
                warnf(w, "%s: changed on the host since start, the game's deletion was discarded", me->fat);
            } else if (!DeleteFileW(me->host)) {
                warnf(w, "%s: cannot delete from the directory (error %lu)", me->fat, (unsigned long)GetLastError());
            }
        }
    }
    smap_free(&map);
    ilist_free(&L);
}

int sddir_sync(const fatvol_io* io, sddir_warn_fn warn, void* wctx)
{
    wsink w;
    fatvol* v;
    char err[256] = "";
    if (!g_have) return 0;
    g_have = 0; /* once */
    w.fn = warn; w.ctx = wctx; w.count = 0;
    v = fatvol_open(io, err, sizeof err);
    if (!v) {
        warnf(&w, "the SD image could not be read (%s); the directory was left as it is", err);
    } else {
        sync_core(v, &g_m, &w);
        fatvol_close(v);
    }
    m_free(&g_m);
    return w.count;
}

int sddir_export(const fatvol_io* io, const char* dir, sddir_warn_fn warn, void* wctx)
{
    wsink w;
    fatvol* v;
    manifest m;
    char err[256] = "";
    wchar_t* wd = u2w(dir);
    if (!wd) return -1;
    v = fatvol_open(io, err, sizeof err);
    if (!v) { free(wd); return -1; }
    w.fn = warn; w.ctx = wctx; w.count = 0;
    memset(&m, 0, sizeof m);
    m.root = wd;
    mkdir_p(wd);
    sync_core(v, &m, &w);
    fatvol_close(v);
    m_free(&m);
    return w.count;
}
