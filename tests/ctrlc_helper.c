/* Test helper: runs a command in its own console process group, sends it CTRL_BREAK after
   a delay (the console handlers of a game exe treat Ctrl+C and Ctrl+Break alike) and returns
   its exit code. usage: ctrlc_helper <delay_ms> <exe> [args...]
   Exit 98 = bad usage / cannot start, 99 = the event could not be sent, 97 = child did not end. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    char cmd[4096];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 0;
    int i;
    size_t n = 0;
    if (argc < 3) { fprintf(stderr, "usage: ctrlc_helper <delay_ms> <exe> [args...]\n"); return 98; }
    cmd[0] = 0;
    for (i = 2; i < argc; i++) {
        n += (size_t)snprintf(cmd + n, sizeof cmd - n, "%s\"%s\"", i > 2 ? " " : "", argv[i]);
        if (n >= sizeof cmd) return 98;
    }
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "cannot start %s\n", argv[2]);
        return 98;
    }
    Sleep((DWORD)atoi(argv[1]));
    if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId)) {
        fprintf(stderr, "GenerateConsoleCtrlEvent failed (%lu)\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 99;
    }
    if (WaitForSingleObject(pi.hProcess, 30000) != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 1);
        return 97;
    }
    GetExitCodeProcess(pi.hProcess, &code);
    return (int)code;
}
