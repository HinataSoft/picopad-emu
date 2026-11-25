/* Input script parser and player. Pure C: no SDL, no SDK headers. */
#include "script.h"

#include "plat_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TIME_MS 1000000000000LL /* keeps t_ms * 1000 far from overflow */

static const struct { const char* name; int code; } key_table[] = {
    { "UP", 1 }, { "LEFT", 2 }, { "RIGHT", 3 }, { "DOWN", 4 },
    { "X", 5 }, { "Y", 6 }, { "A", 7 }, { "B", 8 },
};

static void set_err(char* err, int errlen, int line, const char* msg)
{
    if (err && errlen > 0)
        snprintf(err, (size_t)errlen, "line %d: %s", line, msg);
}

static int add_event(script* s, int* cap, long long t_ms, script_op op,
                     unsigned int key_bit, const char* name, int line)
{
    script_event e;
    int i;
    if (s->count == *cap) {
        int ncap = *cap ? *cap * 2 : 16;
        script_event* p = (script_event*)realloc(s->ev, (size_t)ncap * sizeof *p);
        if (!p) return 0;
        s->ev = p;
        *cap = ncap;
    }
    memset(&e, 0, sizeof e);
    e.t_ms = t_ms;
    e.op = op;
    e.key_bit = key_bit;
    if (name) strncpy(e.name, name, sizeof e.name - 1);
    e.line = line;
    /* stable insertion: after every event with t_ms <= this one */
    i = s->count;
    while (i > 0 && s->ev[i - 1].t_ms > t_ms) {
        s->ev[i] = s->ev[i - 1];
        i--;
    }
    s->ev[i] = e;
    s->count++;
    return 1;
}

/* next whitespace-separated token; returns its length (0 = none, comment or end of line) */
static size_t token(const char** p, const char** start)
{
    const char* q = *p;
    size_t n = 0;
    while (*q == ' ' || *q == '\t' || *q == '\r') q++;
    if (*q == '#' || *q == '\n' || *q == '\0') { *p = q; *start = q; return 0; }
    *start = q;
    while (q[n] && q[n] != ' ' && q[n] != '\t' && q[n] != '\r' &&
           q[n] != '\n' && q[n] != '#')
        n++;
    *p = q + n;
    return n;
}

static int parse_num(const char* t, size_t n, long long* out)
{
    long long v = 0;
    size_t i;
    if (n == 0) return 0;
    for (i = 0; i < n; i++) {
        if (t[i] < '0' || t[i] > '9') return 0;
        v = v * 10 + (t[i] - '0');
        if (v > MAX_TIME_MS) return 0;
    }
    *out = v;
    return 1;
}

static int upper(char c)
{
    return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;
}

static int parse_key(const char* t, size_t n, unsigned int* bit)
{
    size_t k, i;
    for (k = 0; k < sizeof key_table / sizeof key_table[0]; k++) {
        const char* nm = key_table[k].name;
        if (strlen(nm) != n) continue;
        for (i = 0; i < n; i++)
            if (upper(t[i]) != nm[i]) break;
        if (i == n) { *bit = PLAT_KEY_BIT(key_table[k].code); return 1; }
    }
    return 0;
}

static int name_char_ok(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
}

static int parse_line(script* s, int* cap, const char* p, int line,
                      char* err, int errlen)
{
    const char *t, *c;
    size_t n, cn;
    long long ms, dur;
    unsigned int bit;
    char name[64];
    const char* msg;

    n = token(&p, &t);
    if (n == 0) return 0; /* blank or comment */
    if (!parse_num(t, n, &ms)) { msg = "invalid time"; goto fail; }
    cn = token(&p, &c);
    if (cn == 0) { msg = "missing command"; goto fail; }

#define IS_CMD(w) (cn == sizeof(w) - 1 && strncmp(c, w, cn) == 0)
    if (IS_CMD("press") || IS_CMD("release")) {
        int press = (c[0] == 'p');
        n = token(&p, &t);
        if (n == 0) { msg = "missing key"; goto fail; }
        if (!parse_key(t, n, &bit)) { msg = "unknown key"; goto fail; }
        if (token(&p, &t)) { msg = "unexpected extra argument"; goto fail; }
        if (!add_event(s, cap, ms, press ? SCR_PRESS : SCR_RELEASE, bit, NULL, line))
            { msg = "out of memory"; goto fail; }
    } else if (IS_CMD("hold")) {
        n = token(&p, &t);
        if (n == 0) { msg = "missing key"; goto fail; }
        if (!parse_key(t, n, &bit)) { msg = "unknown key"; goto fail; }
        n = token(&p, &t);
        if (n == 0) { msg = "missing duration"; goto fail; }
        if (!parse_num(t, n, &dur) || dur <= 0 || ms + dur > MAX_TIME_MS)
            { msg = "invalid duration"; goto fail; }
        if (token(&p, &t)) { msg = "unexpected extra argument"; goto fail; }
        if (!add_event(s, cap, ms, SCR_PRESS, bit, NULL, line) ||
            !add_event(s, cap, ms + dur, SCR_RELEASE, bit, NULL, line))
            { msg = "out of memory"; goto fail; }
    } else if (IS_CMD("shot")) {
        size_t i;
        n = token(&p, &t);
        if (n == 0) { msg = "missing shot name"; goto fail; }
        if (n > 63) { msg = "shot name too long"; goto fail; }
        for (i = 0; i < n; i++)
            if (!name_char_ok(t[i])) { msg = "invalid shot name"; goto fail; }
        memcpy(name, t, n);
        name[n] = '\0';
        if (token(&p, &t)) { msg = "unexpected extra argument"; goto fail; }
        if (!add_event(s, cap, ms, SCR_SHOT, 0, name, line))
            { msg = "out of memory"; goto fail; }
    } else if (IS_CMD("quit")) {
        if (token(&p, &t)) { msg = "unexpected extra argument"; goto fail; }
        if (!add_event(s, cap, ms, SCR_QUIT, 0, NULL, line))
            { msg = "out of memory"; goto fail; }
    } else {
        msg = "unknown command";
        goto fail;
    }
#undef IS_CMD
    return 0;
fail:
    set_err(err, errlen, line, msg);
    return 1;
}

int script_parse(const char* text, script* out, char* err, int errlen)
{
    int cap = 0, line = 1;
    const char* p = text;
    memset(out, 0, sizeof *out);
    if (err && errlen > 0) err[0] = '\0';
    while (*p) {
        if (parse_line(out, &cap, p, line, err, errlen)) {
            script_free(out);
            return 1;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        line++;
    }
    return 0;
}

int script_load(const char* path, script* out, char* err, int errlen)
{
    FILE* f;
    long size;
    char* buf;
    int rc;
    memset(out, 0, sizeof *out);
    f = fopen(path, "rb");
    if (!f || fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0) {
        if (f) fclose(f);
        if (err && errlen > 0) snprintf(err, (size_t)errlen, "cannot read script '%s'", path);
        return 1;
    }
    buf = (char*)malloc((size_t)size + 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        if (err && errlen > 0) snprintf(err, (size_t)errlen, "cannot read script '%s'", path);
        return 1;
    }
    fclose(f);
    buf[size] = '\0';
    rc = script_parse(buf, out, err, errlen);
    free(buf);
    return rc;
}

void script_free(script* s)
{
    free(s->ev);
    memset(s, 0, sizeof *s);
}

int script_step(script* s, unsigned long long t_us, const script_sink* sink)
{
    s->pressed_this_tick = 0;
    while (s->next < s->count) {
        const script_event* e = &s->ev[s->next];
        if ((unsigned long long)e->t_ms * 1000ull > t_us) break;
        if (e->op == SCR_RELEASE && (s->pressed_this_tick & e->key_bit))
            break; /* deferred to the next step; later events keep their order */
        s->next++;
        switch (e->op) {
        case SCR_PRESS:
            s->keys |= e->key_bit;
            s->pressed_this_tick |= e->key_bit;
            break;
        case SCR_RELEASE:
            s->keys &= ~e->key_bit;
            break;
        case SCR_SHOT:
            if (sink && sink->shot) sink->shot(sink->ctx, e->name);
            break;
        case SCR_QUIT:
            return 1;
        }
    }
    return 0;
}

int script_pending(const script* s)
{
    return s->count - s->next;
}
