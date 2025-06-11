#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <signal.h>
#include <errno.h>

void anti_debug() {
    // Prevent future ptrace attachments (Linux-specific)
    prctl(PR_SET_DUMPABLE, 0);           // disables gdb attaching
    prctl(PR_SET_PTRACER, -1);           // deny any future ptrace requests

    // Detect if we're already being traced
    if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
        if (errno == EPERM) {
            // Already being traced!
            fprintf(stderr, "Debugger detected. Exiting.\n");
            kill(getpid(), SIGKILL);
        }
    }
}
