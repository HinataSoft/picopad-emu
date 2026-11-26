/* Helper exe of the sddir tests:
     sddir_tool build DIR IMG [--size MB]   import DIR into an in-memory image, write the image to IMG
     sddir_tool export IMG DIR              read the image file IMG, write its whole tree into DIR
   Warnings go to stderr as "sddir_tool: <text>". Exit: 0 ok, 1 error, 2 arguments. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../platform/fatvol.h"
#include "../platform/sddir.h"
#include "../platform/sparsedisk.h"

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

static void on_warn(void* ctx, const char* msg)
{
    (void)ctx;
    fprintf(stderr, "sddir_tool: %s\n", msg);
}

static int usage(void)
{
    fprintf(stderr, "usage: sddir_tool build DIR IMG [--size MB]\n"
                    "       sddir_tool export IMG DIR\n");
    return 2;
}

static int do_build(const char* dir, const char* img, unsigned long mb)
{
    char err[256] = "";
    unsigned int sectors = (unsigned int)(mb * 2048ul), s;
    sparsedisk* d = sparse_create(sectors);
    fatvol_io io;
    FILE* f;
    unsigned char buf[512];
    int rc = 1;
    if (!d) { fprintf(stderr, "sddir_tool: out of memory\n"); return 1; }
    io = make_io(d);
    if (sddir_import(dir, &io, on_warn, NULL, err, sizeof err)) {
        fprintf(stderr, "sddir_tool: %s\n", err);
        sparse_free(d);
        return 1;
    }
    f = fopen(img, "wb");
    if (!f) { fprintf(stderr, "sddir_tool: cannot create %s\n", img); sparse_free(d); return 1; }
    rc = 0;
    for (s = 0; s < sectors && !rc; s++) {
        if (!sparse_read(d, s, buf) || fwrite(buf, 1, 512, f) != 512) rc = 1;
    }
    if (fclose(f)) rc = 1;
    if (rc) fprintf(stderr, "sddir_tool: writing %s failed\n", img);
    sparse_free(d);
    return rc;
}

static int do_export(const char* img, const char* dir)
{
    FILE* f = fopen(img, "rb");
    long len;
    unsigned int sectors, s;
    sparsedisk* d;
    unsigned char buf[512];
    fatvol_io io;
    int rc;
    if (!f) { fprintf(stderr, "sddir_tool: cannot open %s\n", img); return 1; }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len % 512) { fclose(f); fprintf(stderr, "sddir_tool: bad image size\n"); return 1; }
    sectors = (unsigned int)(len / 512);
    d = sparse_create(sectors);
    if (!d) { fclose(f); fprintf(stderr, "sddir_tool: out of memory\n"); return 1; }
    for (s = 0; s < sectors; s++) {
        if (fread(buf, 1, 512, f) != 512 || !sparse_write(d, s, buf)) {
            fprintf(stderr, "sddir_tool: reading %s failed\n", img);
            fclose(f);
            sparse_free(d);
            return 1;
        }
    }
    fclose(f);
    io = make_io(d);
    rc = sddir_export(&io, dir, on_warn, NULL);
    sparse_free(d);
    return rc == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
    if (argc >= 4 && !strcmp(argv[1], "build")) {
        unsigned long mb = 1024;
        if (argc == 6 && !strcmp(argv[4], "--size")) {
            char* e;
            mb = strtoul(argv[5], &e, 10);
            if (*e || mb == 0 || mb > 2048) return usage();
        } else if (argc != 4) return usage();
        return do_build(argv[2], argv[3], mb);
    }
    if (argc == 4 && !strcmp(argv[1], "export")) return do_export(argv[2], argv[3]);
    return usage();
}
