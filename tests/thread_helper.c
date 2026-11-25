/* Tiny thread helper for tests (no SDK headers, so windows.h is fine here). */
#include <windows.h>

typedef void (*thr_fn)(void*);

struct thr { thr_fn fn; void* arg; HANDLE h; };

static DWORD WINAPI thr_main(LPVOID p)
{
    struct thr* t = (struct thr*)p;
    t->fn(t->arg);
    return 0;
}

void* thr_start(thr_fn fn, void* arg)
{
    struct thr* t = (struct thr*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*t));
    t->fn = fn;
    t->arg = arg;
    t->h = CreateThread(NULL, 0, thr_main, t, 0, NULL);
    return t;
}

/* returns 1 if the thread finished within timeout_ms, 0 on timeout */
int thr_join(void* p, int timeout_ms)
{
    struct thr* t = (struct thr*)p;
    return WaitForSingleObject(t->h, (DWORD)timeout_ms) == WAIT_OBJECT_0;
}

/* real monotonic time [ms] (bounds helper-thread loops) */
unsigned long long thr_ms(void)
{
    return (unsigned long long)GetTickCount64();
}
