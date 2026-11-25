#include "check.h"
#include "script.h"
#include "plat_api.h"

#include <stdio.h>
#include <string.h>

static char last_shot[64];
static int shot_count;
static void on_shot(void* ctx, const char* name)
{
    (void)ctx;
    snprintf(last_shot, sizeof last_shot, "%s", name);
    shot_count++;
}

int main(void)
{
    script s, s2, s3;
    char err[128];
    script_sink sink = { on_shot, NULL };

    CHECK_EQ(script_parse("500 press A\n600 release a\n# c\n\n2000 shot title\n2000 hold RIGHT 300\n9000 quit\n", &s, err, 128), 0);
    CHECK_EQ(s.count, 6);
    CHECK_EQ(script_pending(&s), 6);
    script_step(&s, 499000, &sink); CHECK_EQ(s.keys, 0);
    script_step(&s, 500000, &sink); CHECK_EQ(s.keys, PLAT_KEY_BIT(7));
    script_step(&s, 600000, &sink); CHECK_EQ(s.keys, 0);
    script_step(&s, 2000000, &sink); CHECK(strcmp(last_shot, "title") == 0); CHECK_EQ(s.keys, PLAT_KEY_BIT(3));
    script_step(&s, 2300000, &sink); CHECK_EQ(s.keys, 0);
    CHECK_EQ(script_pending(&s), 1);
    CHECK_EQ(script_step(&s, 9000000, &sink), 1);
    script_free(&s);

    /* same-tick tap */
    CHECK_EQ(script_parse("500 press A\n500 release A\n", &s2, err, 128), 0);
    script_step(&s2, 500000, &sink); CHECK_EQ(s2.keys, PLAT_KEY_BIT(7));
    CHECK_EQ(script_pending(&s2), 1);
    script_step(&s2, 505000, &sink); CHECK_EQ(s2.keys, 0);
    CHECK_EQ(script_pending(&s2), 0);
    script_free(&s2);

    /* unsorted input, CRLF, trailing whitespace, trailing comment, quit stops step */
    CHECK_EQ(script_parse("20 quit  \r\n10 press up # hi\r\n10 shot a-1_B\r\n30 shot late\r\n", &s2, err, 128), 0);
    shot_count = 0;
    CHECK_EQ(script_step(&s2, 40000, &sink), 1);
    CHECK_EQ(s2.keys, PLAT_KEY_BIT(1));
    CHECK_EQ(shot_count, 1);
    CHECK_EQ(script_pending(&s2), 1);
    script_free(&s2);

    /* errors */
    CHECK_EQ(script_parse("100 jump A\n", &s3, err, 128), 1); CHECK(strstr(err, "line 1") != NULL);
    CHECK_EQ(script_parse("\n\nx press A\n", &s3, err, 128), 1); CHECK(strstr(err, "line 3") != NULL);
    CHECK_EQ(script_parse("100 press Q\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("100 shot a/b\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("100 hold A 0\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("100 hold A -5\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("-1 press A\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("100 press\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("100\n", &s3, err, 128), 1);
    CHECK_EQ(script_parse("100 quit now\n", &s3, err, 128), 1);
    CHECK_EQ(script_load("no/such/file.scr", &s3, err, 128), 1);
    CHECK(strstr(err, "no/such/file.scr") != NULL);
    return CHECK_DONE();
}
