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
    /* headless defaults */
    {
        char* argv[] = { "x.exe" };
        CHECK_EQ(parse(&o, err, 1, argv), CLI_OK);
        CHECK_EQ(o.headless, 0);
        CHECK(o.script == NULL);
        CHECK(o.wav == NULL);
        CHECK(strcmp(o.shot_dir, ".") == 0);
        CHECK_EQ(o.trace_keys, 0);
        CHECK_EQ(o.max_time_set, 0);
        CHECK_EQ(o.max_time_ms, 0);
    }
    {
        char* argv[] = { "x.exe", "--headless" };
        CHECK_EQ(parse(&o, err, 2, argv), CLI_OK);
        CHECK_EQ(o.headless, 1);
        CHECK_EQ(o.max_time_ms, 60000);
        CHECK_EQ(o.max_time_set, 0);
        CHECK_EQ(o.seed, 0);
        CHECK_EQ(o.seed_set, 0);
    }
    {   /* option order does not matter */
        char* argv[] = { "x.exe", "--max-time", "5s", "--seed", "9", "--headless" };
        CHECK_EQ(parse(&o, err, 6, argv), CLI_OK);
        CHECK_EQ(o.max_time_ms, 5000);
        CHECK_EQ(o.max_time_set, 1);
        CHECK_EQ(o.seed, 9);
        CHECK_EQ(o.seed_set, 1);
    }
    {
        char* argv[] = { "x.exe", "--headless", "--script", "s.txt", "--shot-dir", "out",
                         "--wav", "a.wav", "--trace-keys", "--scale", "2", "--keys", "layout" };
        CHECK_EQ(parse(&o, err, 13, argv), CLI_OK);
        CHECK(strcmp(o.script, "s.txt") == 0);
        CHECK(strcmp(o.shot_dir, "out") == 0);
        CHECK(strcmp(o.wav, "a.wav") == 0);
        CHECK_EQ(o.trace_keys, 1);
        CHECK_EQ(o.scale, 2);
        CHECK_EQ(o.keys, KEYMAP_LAYOUT);
    }
    {   /* --opt=value form */
        char* argv[] = { "x.exe", "--headless", "--script=s.txt", "--wav=a.wav" };
        CHECK_EQ(parse(&o, err, 4, argv), CLI_OK);
        CHECK(strcmp(o.script, "s.txt") == 0);
        CHECK(strcmp(o.wav, "a.wav") == 0);
    }
    {   /* --trace-keys works without headless; interactive keeps defaults */
        char* argv[] = { "x.exe", "--trace-keys" };
        CHECK_EQ(parse(&o, err, 2, argv), CLI_OK);
        CHECK_EQ(o.trace_keys, 1);
        CHECK_EQ(o.headless, 0);
        CHECK_EQ(o.max_time_ms, 0);
    }
    {   /* --wav needs --headless, in either order */
        char* argv[] = { "x.exe", "--wav", "a.wav" };
        CHECK_EQ(parse(&o, err, 3, argv), CLI_ERROR);
        CHECK(strstr(err, "--headless") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--wav", "a.wav", "--headless" };
        CHECK_EQ(parse(&o, err, 4, argv), CLI_OK);
    }
    {   /* --script and --shot-dir need --headless too, in either order */
        char* argv[] = { "x.exe", "--script", "s.txt" };
        CHECK_EQ(parse(&o, err, 3, argv), CLI_ERROR);
        CHECK(strstr(err, "--script") != NULL);
        CHECK(strstr(err, "--headless") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--shot-dir=out" };
        CHECK_EQ(parse(&o, err, 2, argv), CLI_ERROR);
        CHECK(strstr(err, "--shot-dir") != NULL);
        CHECK(strstr(err, "--headless") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--shot-dir", "out", "--script", "s.txt", "--headless" };
        CHECK_EQ(parse(&o, err, 6, argv), CLI_OK);
    }
    {
        char* argv[] = { "x.exe", "--headless", "--script" };
        CHECK_EQ(parse(&o, err, 3, argv), CLI_ERROR);
        CHECK(strstr(err, "--script") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--shot-dir" };
        CHECK_EQ(parse(&o, err, 2, argv), CLI_ERROR);
        CHECK(strstr(err, "--shot-dir") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--headless", "--wav" };
        CHECK_EQ(parse(&o, err, 3, argv), CLI_ERROR);
        CHECK(strstr(err, "--wav") != NULL);
    }
    {   /* empty value */
        char* argv[] = { "x.exe", "--headless", "--script=" };
        CHECK_EQ(parse(&o, err, 3, argv), CLI_ERROR);
        CHECK(strstr(err, "--script") != NULL);
    }
    {
        char* argv[] = { "x.exe", "--headless=1" };   /* flag takes no value */
        CHECK_EQ(parse(&o, err, 2, argv), CLI_ERROR);
    }
    /* help */
    {
        char* argv[] = { "x.exe", "--help" };
        CHECK_EQ(parse(&o, err, 2, argv), CLI_HELP);
        CHECK(strstr(err, "--max-time") != NULL);
    }
    {   /* full usage text (err buffer above is small) lists the headless options */
        char big[1024];
        char* argv[] = { "x.exe", "--help" };
        CHECK_EQ(cli_parse(2, argv, &o, big, sizeof(big)), CLI_HELP);
        CHECK(strstr(big, "--headless") != NULL);
        CHECK(strstr(big, "--script") != NULL);
        CHECK(strstr(big, "--shot-dir") != NULL);
        CHECK(strstr(big, "--wav") != NULL);
        CHECK(strstr(big, "--trace-keys") != NULL);
        CHECK(strstr(cli_usage(), "--trace-keys") != NULL);
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
