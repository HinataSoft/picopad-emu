/* platform/cli: parsing of the game .exe command line */
#include <string.h>
#include "check.h"
#include "../platform/cli.h"

static int parse(cli_options* o, char* err, int argc, char** argv)
{
    memset(o, 0x5A, sizeof(*o));
    err[0] = 0;
    return cli_parse(argc, argv, o, err, 256);
}

int main(void)
{
    cli_options o;
    char err[256];

    /* brief: all options */
    {
        char* argv[] = { "x.exe", "--scale", "2", "--keys", "layout", "--fast-display",
                         "--seed", "7", "--max-time", "2s" };
        CHECK_EQ(parse(&o, err, 10, argv), CLI_OK);
        CHECK_EQ(o.scale, 2);
        CHECK_EQ(o.keys, KEYMAP_LAYOUT);
        CHECK_EQ(o.fast_display, 1);
        CHECK_EQ(o.seed, 7);
        CHECK_EQ(o.seed_set, 1);
        CHECK_EQ(o.max_time_ms, 2000);
    }
    /* defaults */
    {
        char* argv[] = { "x.exe" };
        CHECK_EQ(parse(&o, err, 1, argv), CLI_OK);
        CHECK_EQ(o.scale, 3);
        CHECK_EQ(o.keys, KEYMAP_LETTERS);
        CHECK_EQ(o.fast_display, 0);
        CHECK_EQ(o.seed_set, 0);
        CHECK_EQ(o.max_time_ms, 0);
    }
    /* --keys foo -> error mentioning --keys */
    {
        char* argv[] = { "x.exe", "--keys", "foo" };
        CHECK(parse(&o, err, 3, argv) != CLI_OK);
        CHECK(strstr(err, "--keys") != NULL);
    }
    /* --opt=value form, max-time formats */
    {
        char* argv[] = { "x.exe", "--keys=letters", "--scale=1", "--max-time=2000" };
        CHECK_EQ(parse(&o, err, 4, argv), CLI_OK);
        CHECK_EQ(o.keys, KEYMAP_LETTERS);
        CHECK_EQ(o.scale, 1);
        CHECK_EQ(o.max_time_ms, 2000);
    }
    {
        char* argv[] = { "x.exe", "--max-time", "1500ms" };
        CHECK_EQ(parse(&o, err, 3, argv), CLI_OK);
        CHECK_EQ(o.max_time_ms, 1500);
    }
    /* errors */
    {
        char* argv[] = { "x.exe", "--max-time", "2h" };
        CHECK(parse(&o, err, 3, argv) != CLI_OK);
        CHECK(strstr(err, "--max-time") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--scale", "0" };
        CHECK(parse(&o, err, 3, argv) != CLI_OK);
        CHECK(strstr(err, "--scale") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--scale" };   /* missing value */
        CHECK(parse(&o, err, 2, argv) != CLI_OK);
        CHECK(strstr(err, "--scale") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--bogus" };
        CHECK(parse(&o, err, 2, argv) != CLI_OK);
        CHECK(strstr(err, "--bogus") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--seed", "abc" };
        CHECK(parse(&o, err, 3, argv) != CLI_OK);
        CHECK(strstr(err, "--seed") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--fast-display=1" };  /* flag takes no value */
        CHECK(parse(&o, err, 2, argv) != CLI_OK);
    }
    /* help */
    {
        char* argv[] = { "x.exe", "--help" };
        CHECK_EQ(parse(&o, err, 2, argv), CLI_HELP);
        CHECK(strstr(err, "--max-time") != NULL);
    }
    /* small err buffer is NUL-terminated */
    {
        char small[8];
        char* argv[] = { "x.exe", "--keys", "foo" };
        memset(small, 'z', sizeof(small));
        CHECK(cli_parse(3, argv, &o, small, sizeof(small)) != CLI_OK);
        CHECK(memchr(small, 0, sizeof(small)) != NULL);
    }
    return CHECK_DONE();
}
