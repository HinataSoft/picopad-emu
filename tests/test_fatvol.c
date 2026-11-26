/* Test of platform/fatvol: FAT32 formatter, writer and reader over sparsedisk. */
#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "../platform/fatvol.h"
#include "../platform/sparsedisk.h"

#define SECTORS_1G 2097152u

/* ---- storage adapter ---- */
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

static unsigned int rd16(const unsigned char* p) { return p[0] | (p[1] << 8); }
static unsigned int rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

/* ---- expected cluster size, derived independently from DiskFormat's rule:
   mediasize = total - 63; clustnum = (mediasize - 32 - fat) / clust with
   fat = (clustnum0 * 4 + 8 + 511) / 512 computed twice; accepted if clustnum > 0xFFF8. */
static unsigned int expected_clust(unsigned int total)
{
    unsigned int media = total - 63, c;
    for (c = 32; c > 0; c >>= 1) {
        unsigned int n = media / c;
        unsigned int fat = (n * 4 + 8 + 511) / 512;
        n = (media - 32 - fat) / c;
        if (n > 0xFFF8u) return c;
    }
    return 0;
}

/* ---- data pattern ---- */
static void fill(unsigned char* b, unsigned int n, unsigned int seed, unsigned int off)
{
    unsigned int i;
    for (i = 0; i < n; i++) b[i] = (unsigned char)(((off + i) * 31u + seed * 17u + ((off + i) >> 8)) & 0xFF);
}

static fat_stamp stamp(unsigned int n)
{
    fat_stamp t;
    t.date = (unsigned short)(((2026 - 1980) << 9) | (((n % 12) + 1) << 5) | ((n % 28) + 1));
    t.time = (unsigned short)(((n % 24) << 11) | ((n % 60) << 5) | (n % 30));
    return t;
}

static int put(fatvol* v, const char* path, unsigned int size, unsigned int seed, fat_stamp t)
{
    unsigned char buf[1000];
    unsigned int off = 0;
    int r = fatvol_file_begin(v, path, size, t);
    if (r) return r;
    while (off < size) {
        unsigned int n = size - off > sizeof(buf) ? (unsigned int)sizeof(buf) : size - off;
        fill(buf, n, seed, off);
        r = fatvol_file_write(v, buf, n);
        if (r) return r;
        off += n;
    }
    return fatvol_file_end(v);
}

/* ---- walk collector ---- */
typedef struct {
    char path[80];
    int is_dir;
    unsigned int size, first_cluster;
    fat_stamp st;
} rec;
typedef struct {
    rec r[1024];
    int n;
} recs;

static int collect(void* ctx, const fatvol_entry* e)
{
    recs* R = (recs*)ctx;
    if (R->n >= 1024) return 1;
    strncpy(R->r[R->n].path, e->path, 79);
    R->r[R->n].path[79] = 0;
    R->r[R->n].is_dir = e->is_dir;
    R->r[R->n].size = e->size;
    R->r[R->n].first_cluster = e->first_cluster;
    R->r[R->n].st = e->stamp;
    R->n++;
    return 0;
}

static const rec* find_rec(const recs* R, const char* path)
{
    int i;
    for (i = 0; i < R->n; i++)
        if (strcmp(R->r[i].path, path) == 0) return &R->r[i];
    return NULL;
}

typedef struct {
    unsigned int seed, off;
    int bad;
} cmpctx;

static int cmp_sink(void* ctx, const void* data, unsigned int n)
{
    cmpctx* c = (cmpctx*)ctx;
    unsigned char exp[4096];
    const unsigned char* p = (const unsigned char*)data;
    while (n) {
        unsigned int k = n > sizeof(exp) ? (unsigned int)sizeof(exp) : n;
        fill(exp, k, c->seed, c->off);
        if (memcmp(exp, p, k) != 0) c->bad = 1;
        c->off += k;
        p += k;
        n -= k;
    }
    return 0;
}

static void check_content(fatvol* v, const recs* R, const char* path, unsigned int seed)
{
    const rec* r = find_rec(R, path);
    fatvol_entry e;
    cmpctx c;
    CHECK(r != NULL);
    if (!r) return;
    memset(&e, 0, sizeof(e));
    e.path = r->path;
    e.is_dir = r->is_dir;
    e.size = r->size;
    e.stamp = r->st;
    e.first_cluster = r->first_cluster;
    c.seed = seed;
    c.off = 0;
    c.bad = 0;
    CHECK_EQ(fatvol_read_file(v, &e, cmp_sink, &c), 0);
    CHECK_EQ(c.bad, 0);
    CHECK_EQ(c.off, r->size);
}

/* ---- tests ---- */
static void test_layout(sparsedisk* d, unsigned int total)
{
    unsigned char s0[512], b[512], b2[512];
    unsigned int pstart, psize, clust = expected_clust(total), fatsz, nclust;
    CHECK(clust != 0);
    CHECK(sparse_read(d, 0, s0));
    CHECK_EQ(s0[510], 0x55);
    CHECK_EQ(s0[511], 0xAA);
    CHECK_EQ(s0[446 + 4], 0x0C);
    pstart = rd32(s0 + 446 + 8);
    psize = rd32(s0 + 446 + 12);
    CHECK_EQ(pstart, 63);
    CHECK(psize <= total - 63);
    CHECK(sparse_read(d, pstart, b));
    CHECK_EQ(b[510], 0x55);
    CHECK_EQ(b[511], 0xAA);
    CHECK_EQ(rd16(b + 11), 512);
    CHECK_EQ(b[13], clust);
    CHECK_EQ(rd16(b + 14), 32);
    CHECK_EQ(b[16], 1);
    CHECK_EQ(rd16(b + 17), 0);
    CHECK_EQ(rd16(b + 19), 0);
    CHECK_EQ(rd32(b + 32), psize);
    CHECK_EQ(rd32(b + 28), 63);
    fatsz = rd32(b + 36);
    CHECK_EQ(rd32(b + 44), 2);
    CHECK_EQ(rd16(b + 48), 1);
    CHECK_EQ(rd16(b + 50), 6);
    CHECK_EQ(memcmp(b + 82, "FAT32   ", 8), 0);
    nclust = (psize - 32 - fatsz) / clust;
    CHECK(nclust > 0xFFF8u);
    CHECK_EQ(psize, 32 + fatsz + nclust * clust);
    CHECK(sparse_read(d, pstart + 6, b2));
    CHECK_EQ(memcmp(b, b2, 512), 0);
    CHECK(sparse_read(d, pstart + 1, b));
    CHECK_EQ(rd32(b), 0x41615252u);
    CHECK_EQ(rd32(b + 0x1E4), 0x61417272u);
    CHECK_EQ(rd32(b + 0x1E8), nclust - 1);
    CHECK_EQ(rd32(b + 0x1EC), 2);
    CHECK_EQ(b[510], 0x55);
    CHECK_EQ(b[511], 0xAA);
    CHECK(sparse_read(d, pstart + 32, b));
    CHECK_EQ(rd32(b + 0), 0xFFFFFFF8u);
    CHECK_EQ(rd32(b + 4), 0xFFFFFFFFu);
    CHECK_EQ(rd32(b + 8), 0x0FFFFFFFu);
    CHECK_EQ(rd32(b + 12), 0);
    fprintf(stdout, "total=%u sectors: SecPerClus=%u FATsz=%u clusters=%u\n", total, clust, fatsz, nclust);
}

static void test_main_flow(void)
{
    char err[200] = "";
    sparsedisk* d = sparse_create(SECTORS_1G);
    fatvol_io io = make_io(d);
    fatvol* v;
    recs* R = (recs*)malloc(sizeof(recs));
    unsigned char sec[512];
    unsigned int cb;
    unsigned int pstart;

    CHECK(d && R);
    v = fatvol_format(&io, err, sizeof(err));
    CHECK(v != NULL);
    if (!v) return;
    CHECK(sparse_read(d, 0, sec));
    pstart = rd32(sec + 446 + 8);
    CHECK(sparse_read(d, pstart, sec));
    cb = sec[13] * 512u;
    test_layout(d, SECTORS_1G);

    CHECK_EQ(fatvol_mkdir(v, "DATA", stamp(1)), 0);
    CHECK_EQ(fatvol_mkdir(v, "DATA/SUB", stamp(2)), 0);
    CHECK(fatvol_mkdir(v, "NOPE/X", stamp(3)) != 0);
    CHECK(fatvol_mkdir(v, "DATA", stamp(3)) != 0); /* exists */
    CHECK_EQ(put(v, "DATA/A.TXT", 5, 1, stamp(4)), 0);
    CHECK_EQ(put(v, "DATA/SUB/EMPTY", 0, 2, stamp(5)), 0);
    CHECK_EQ(put(v, "DATA/CLUST.BIN", cb, 3, stamp(6)), 0);
    CHECK_EQ(put(v, "DATA/BIG.BIN", 3 * cb + 1, 4, stamp(7)), 0);
    CHECK_EQ(put(v, "ROOT.TXT", 100, 5, stamp(8)), 0);
    fatvol_close(v);

    v = fatvol_open(&io, err, sizeof(err));
    CHECK(v != NULL);
    if (!v) return;
    R->n = 0;
    CHECK_EQ(fatvol_walk(v, collect, R), 0);
    CHECK_EQ(R->n, 7);
    if (R->n == 7) {
        static const char* names[7] = {"DATA", "DATA/SUB", "DATA/SUB/EMPTY", "DATA/A.TXT",
                                       "DATA/CLUST.BIN", "DATA/BIG.BIN", "ROOT.TXT"};
        int i;
        /* pre-order in creation order within a directory: a directory before its
           contents; DATA/SUB was created before DATA/A.TXT */
        for (i = 0; i < 7; i++) CHECK(strcmp(R->r[i].path, names[i]) == 0);
        CHECK_EQ(R->r[0].is_dir, 1);
        CHECK_EQ(R->r[1].is_dir, 1);
        CHECK_EQ(R->r[2].is_dir, 0);
        CHECK_EQ(R->r[2].size, 0);
        CHECK_EQ(R->r[2].first_cluster, 0);
        CHECK_EQ(R->r[3].size, 5);
        CHECK_EQ(R->r[4].size, cb);
        CHECK_EQ(R->r[5].size, 3 * cb + 1);
        CHECK_EQ(R->r[6].size, 100);
        for (i = 0; i < 7; i++) {
            static const unsigned int st[7] = {1, 2, 5, 4, 6, 7, 8};
            fat_stamp t = stamp(st[i]);
            CHECK_EQ(R->r[i].st.date, t.date);
            CHECK_EQ(R->r[i].st.time, t.time);
        }
    }
    check_content(v, R, "DATA/A.TXT", 1);
    check_content(v, R, "DATA/SUB/EMPTY", 2);
    check_content(v, R, "DATA/CLUST.BIN", 3);
    check_content(v, R, "DATA/BIG.BIN", 4);
    check_content(v, R, "ROOT.TXT", 5);

    /* delete: big file freed and reusable, non-empty dir refused */
    CHECK(fatvol_delete(v, "DATA") != 0);
    CHECK(fatvol_delete(v, "NOPE.TXT") != 0);
    CHECK_EQ(fatvol_delete(v, "DATA/BIG.BIN"), 0);
    R->n = 0;
    CHECK_EQ(fatvol_walk(v, collect, R), 0);
    CHECK_EQ(R->n, 6);
    CHECK(find_rec(R, "DATA/BIG.BIN") == NULL);
    CHECK_EQ(put(v, "DATA/BIG2.BIN", 3 * cb + 1, 9, stamp(9)), 0);
    R->n = 0;
    CHECK_EQ(fatvol_walk(v, collect, R), 0);
    CHECK_EQ(R->n, 7);
    check_content(v, R, "DATA/BIG2.BIN", 9);
    check_content(v, R, "DATA/CLUST.BIN", 3);
    CHECK_EQ(fatvol_delete(v, "DATA/SUB/EMPTY"), 0);
    CHECK_EQ(fatvol_delete(v, "DATA/SUB"), 0); /* now empty */
    fatvol_close(v);

    /* a second open sees the deletions */
    v = fatvol_open(&io, err, sizeof(err));
    CHECK(v != NULL);
    if (v) {
        R->n = 0;
        CHECK_EQ(fatvol_walk(v, collect, R), 0);
        CHECK_EQ(R->n, 5);
        CHECK(find_rec(R, "DATA/SUB") == NULL);
        fatvol_close(v);
    }
    free(R);
    sparse_free(d);
}

static void test_big_root(void)
{
    char err[200] = "";
    sparsedisk* d = sparse_create(SECTORS_1G);
    fatvol_io io = make_io(d);
    fatvol* v = fatvol_format(&io, err, sizeof(err));
    recs* R = (recs*)malloc(sizeof(recs));
    unsigned char sec[512];
    unsigned int cb, count, i;
    char name[32];

    CHECK(v && R);
    if (!v) return;
    CHECK(sparse_read(d, 63, sec));
    cb = sec[13] * 512u;
    count = cb / 32 + 44; /* more entries than one cluster holds */
    CHECK(count < 1000);
    for (i = 0; i < count; i++) {
        sprintf(name, "F%04u.TXT", i);
        CHECK_EQ(put(v, name, i % 40, i, stamp(i)), 0);
    }
    fatvol_close(v);
    v = fatvol_open(&io, err, sizeof(err));
    CHECK(v != NULL);
    if (v) {
        R->n = 0;
        CHECK_EQ(fatvol_walk(v, collect, R), 0);
        CHECK_EQ((unsigned int)R->n, count);
        for (i = 0; i < count && i < (unsigned int)R->n; i++) {
            sprintf(name, "F%04u.TXT", i);
            CHECK(strcmp(R->r[i].path, name) == 0);
            CHECK_EQ(R->r[i].size, i % 40);
        }
        check_content(v, R, "F0299.TXT", 299);
        fatvol_close(v);
    }
    free(R);
    sparse_free(d);
}

static void test_full_disk(void)
{
    char err[200] = "";
    sparsedisk* d = sparse_create(SECTORS_1G);
    fatvol_io io = make_io(d);
    fatvol* v = fatvol_format(&io, err, sizeof(err));
    recs* R = (recs*)malloc(sizeof(recs));
    fat_stamp t = stamp(1);

    CHECK(v && R);
    if (!v) return;
    CHECK_EQ(put(v, "KEEP.TXT", 1234, 7, t), 0);
    CHECK(fatvol_file_begin(v, "HUGE.BIN", 0xFFFFFFFFu, t) != 0);
    CHECK(strstr(fatvol_error(v), "full") != NULL);
    fatvol_close(v);
    v = fatvol_open(&io, err, sizeof(err));
    CHECK(v != NULL);
    if (v) {
        R->n = 0;
        CHECK_EQ(fatvol_walk(v, collect, R), 0);
        CHECK_EQ(R->n, 1);
        if (R->n == 1) CHECK(strcmp(R->r[0].path, "KEEP.TXT") == 0);
        check_content(v, R, "KEEP.TXT", 7);
        fatvol_close(v);
    }
    free(R);
    sparse_free(d);
}

static void test_unformatted(void)
{
    char err[200] = "";
    sparsedisk* d = sparse_create(SECTORS_1G);
    fatvol_io io = make_io(d);
    fatvol* v = fatvol_open(&io, err, sizeof(err));
    CHECK(v == NULL);
    CHECK(err[0] != 0);
    sparse_free(d);
}

static void test_256mb_layout(void)
{
    char err[200] = "";
    unsigned int total = 524288u;
    sparsedisk* d = sparse_create(total);
    fatvol_io io = make_io(d);
    fatvol* v = fatvol_format(&io, err, sizeof(err));
    CHECK(v != NULL);
    if (v) {
        fatvol_close(v);
        test_layout(d, total);
    }
    sparse_free(d);
}

static void test_too_small(void)
{
    char err[200] = "";
    sparsedisk* d = sparse_create(20000u); /* far below 65525 clusters */
    fatvol_io io = make_io(d);
    fatvol* v = fatvol_format(&io, err, sizeof(err));
    CHECK(v == NULL);
    CHECK(err[0] != 0);
    sparse_free(d);
}

int main(void)
{
    test_main_flow();
    test_big_root();
    test_full_disk();
    test_unformatted();
    test_256mb_layout();
    test_too_small();
    return CHECK_DONE();
}
