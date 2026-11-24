/* Command line of a game .exe. Pure C: no SDL, no SDK headers. */
#include "cli.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char Usage[] =
    "Options:\n"
    "  --scale N              window scale 1..8 (default 3 = 960x720)\n"
    "  --keys letters|layout  key mapping (default letters)\n"
    "  --fast-display         no SPI display transfer cost\n"
    "  --seed N               random seed (default from time)\n"
    "  --max-time T           quit with exit code 3 after T (2000, 2000ms or 2s)\n"
    "  --help                 this text";

const char* cli_usage(void)
{
    return Usage;
}

static int fail(char* err, int errlen, const char* fmt, ...)
{
    if (err && errlen > 0)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(err, (size_t)errlen, fmt, args);
        va_end(args);
    }
    return CLI_ERROR;
}

/* unsigned number (base 0: decimal or 0x hex) followed by 'suffix' ("" = none);
   returns 1 when the whole string matches */
static int parse_num(const char* s, const char* suffix, unsigned long long* out)
{
    char* end;
    if (*s < '0' || *s > '9') return 0;   /* no sign, no spaces */
    errno = 0;
    *out = strtoull(s, &end, 0);
    if (errno != 0) return 0;
    return strcmp(end, suffix) == 0;
}

static unsigned int time_seed(void)
{
    unsigned int s = (unsigned int)time(NULL);
    s ^= (unsigned int)clock() * 2654435761u;
    return s;
}

int cli_parse(int argc, char** argv, cli_options* out, char* err, int errlen)
{
    int i;
    if (err && errlen > 0) err[0] = 0;

    out->scale = 3;
    out->keys = KEYMAP_LETTERS;
    out->fast_display = 0;
    out->seed = time_seed();
    out->seed_set = 0;
    out->max_time_ms = 0;

    for (i = 1; i < argc; i++)
    {
        char name[32];
        const char* arg = argv[i];
        const char* val = NULL;
        const char* eq = strchr(arg, '=');
        size_t n = eq ? (size_t)(eq - arg) : strlen(arg);
        int is_flag;

        if (n >= sizeof(name)) n = sizeof(name) - 1;
        memcpy(name, arg, n);
        name[n] = 0;
        if (eq) val = eq + 1;

        if (strcmp(name, "--help") == 0 || strcmp(name, "-h") == 0)
        {
            if (err && errlen > 0) snprintf(err, (size_t)errlen, "%s", Usage);
            return CLI_HELP;
        }

        is_flag = strcmp(name, "--fast-display") == 0;
        if (is_flag)
        {
            if (val) return fail(err, errlen, "%s takes no value", name);
            out->fast_display = 1;
            continue;
        }

        if (strcmp(name, "--scale") && strcmp(name, "--keys") &&
            strcmp(name, "--seed") && strcmp(name, "--max-time"))
            return fail(err, errlen, "unknown option '%s'", arg);

        if (!val)
        {
            if (i + 1 >= argc) return fail(err, errlen, "%s needs a value", name);
            val = argv[++i];
        }

        if (strcmp(name, "--scale") == 0)
        {
            unsigned long long v;
            if (!parse_num(val, "", &v) || v < 1 || v > 8)
                return fail(err, errlen, "--scale: '%s' is not a number 1..8", val);
            out->scale = (int)v;
        }
        else if (strcmp(name, "--keys") == 0)
        {
            if (strcmp(val, "letters") == 0) out->keys = KEYMAP_LETTERS;
            else if (strcmp(val, "layout") == 0) out->keys = KEYMAP_LAYOUT;
            else return fail(err, errlen, "--keys: '%s' (expected letters or layout)", val);
        }
        else if (strcmp(name, "--seed") == 0)
        {
            unsigned long long v;
            if (!parse_num(val, "", &v) || v > 0xFFFFFFFFull)
                return fail(err, errlen, "--seed: '%s' is not a 32-bit unsigned number", val);
            out->seed = (unsigned int)v;
            out->seed_set = 1;
        }
        else /* --max-time */
        {
            unsigned long long v;
            const long long limit = 1000000000000ll;   /* ~31 years in ms */
            if (parse_num(val, "", &v) || parse_num(val, "ms", &v)) { }
            else if (parse_num(val, "s", &v) && v <= (unsigned long long)limit / 1000) v *= 1000;
            else return fail(err, errlen, "--max-time: '%s' (expected 2000, 2000ms or 2s)", val);
            if (v > (unsigned long long)limit)
                return fail(err, errlen, "--max-time: '%s' is too large", val);
            out->max_time_ms = (long long)v;
        }
    }
    return CLI_OK;
}
