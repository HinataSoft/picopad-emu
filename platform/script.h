/* Input script of a headless run: parser and step-wise player.
   Pure C: no SDL, no SDK headers. */
#ifndef PICOPAD_PLATFORM_SCRIPT_H
#define PICOPAD_PLATFORM_SCRIPT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SCR_PRESS, SCR_RELEASE, SCR_SHOT, SCR_QUIT } script_op;

typedef struct {
    long long t_ms;
    script_op op;
    unsigned int key_bit;
    char name[64];
    int line;
} script_event;

typedef struct {
    script_event* ev;
    int count;
    int next;
    unsigned int keys;              /* current key bits */
    unsigned int pressed_this_tick; /* keys pressed in the last script_step */
} script;

/* 0 = OK; otherwise 1 and err = "line N: reason" (N = 1-based source line) */
int script_parse(const char* text, script* out, char* err, int errlen);
/* reads the file, then script_parse; a read error returns 1 and names the path */
int script_load(const char* path, script* out, char* err, int errlen);
void script_free(script* s);

typedef struct {
    void (*shot)(void* ctx, const char* name);
    void* ctx;
} script_sink;

/* Applies events with t_ms*1000 <= t_us in (time-sorted, stable) order.
   A release of a key pressed in the same step is deferred to the next step.
   Returns 1 when quit was reached (later events stay pending). sink may be NULL. */
int script_step(script* s, unsigned long long t_us, const script_sink* sink);
/* number of events not yet applied */
int script_pending(const script* s);

#ifdef __cplusplus
}
#endif

#endif
