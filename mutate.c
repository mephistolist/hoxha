#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <elf.h>
#include <errno.h>

#define MAX_LEN 128

void get_entropy(unsigned char *buf, size_t len) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0 && read(fd, buf, len) == len) {
        close(fd);
        return;
    }
    for (size_t i = 0; i < len; ++i)
        buf[i] = (unsigned char)(__rdtsc() ^ time(NULL) ^ getpid() ^ rand());
    if (fd >= 0) close(fd);
}

void mutate1(char *s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len / 2; ++i) {
        char tmp = s[i];
        s[i] = s[len - i - 1];
        s[len - i - 1] = tmp;
    }
}

void mutate2(char *s) {
    for (size_t i = 0; s[i]; ++i)
        if ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))
            s[i]++;
}

void mutate3(char *s) {
    unsigned char key[1];
    get_entropy(key, 1);
    for (size_t i = 0; s[i]; ++i)
        s[i] ^= key[0];
}

void shuffle(void (**funcs)(char *), int count) {
    for (int i = count - 1; i > 0; --i) {
        unsigned char r[1];
        get_entropy(r, 1);
        int j = r[0] % (i + 1);
        void (*tmp)(char *) = funcs[i];
        funcs[i] = funcs[j];
        funcs[j] = tmp;
    }
}

int run_jit_code() {
    size_t pagesize = sysconf(_SC_PAGE_SIZE);
    unsigned char *mem = mmap(NULL, pagesize,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return -1;

    unsigned char random_val;
    get_entropy(&random_val, 1);

    unsigned char program[] = {
        0x55, 0x48, 0x89, 0xe5,             // push rbp; mov rsp, rbp
        0xb8, random_val, 0x00, 0x00, 0x00, // mov imm8 into eax
        0x5d, 0xc3                          // pop rbp; ret
    };

    size_t offset = 0;
    int nops = rand() % 8;
    for (int i = 0; i < nops; ++i)
        mem[offset++] = 0x90;

    memcpy(mem + offset, program, sizeof(program));
    int (*func)() = (int (*)())mem;
    return func();
}

// Random memory junker
void junk_memory() {
    size_t allocs = 5 + rand() % 5;
    for (size_t i = 0; i < allocs; ++i) {
        size_t size = 512 + rand() % 1024;
        char *buf = malloc(size);
        if (!buf) continue;
        for (size_t j = 0; j < size; ++j) buf[j] = rand() % 256;
        volatile char dummy = buf[rand() % size];
        (void)dummy;
    }
}

// Inline polymorphic junk
void polymorphic_junk() {
    asm volatile (
        "nop\n\t"
        "nop\n\t"
        "nop\n\t"
        "nop\n\t"
    );
}

// Attempt to overwrite .note or .comment (if writable)
void patch_note_section() {
    FILE *f = fopen("/proc/self/exe", "r+b");
    if (!f) return;

    Elf64_Ehdr ehdr;
    fread(&ehdr, 1, sizeof(ehdr), f);
    fseek(f, ehdr.e_shoff, SEEK_SET);

    Elf64_Shdr shdr;
    char shstrtab[4096] = {0};

    fseek(f, ehdr.e_shoff + ehdr.e_shentsize * ehdr.e_shstrndx, SEEK_SET);
    fread(&shdr, 1, sizeof(shdr), f);
    fseek(f, shdr.sh_offset, SEEK_SET);
    fread(shstrtab, 1, sizeof(shstrtab) - 1, f);

    for (int i = 0; i < ehdr.e_shnum; ++i) {
        fseek(f, ehdr.e_shoff + i * ehdr.e_shentsize, SEEK_SET);
        fread(&shdr, 1, sizeof(shdr), f);
        const char *name = &shstrtab[shdr.sh_name];
        if (strcmp(name, ".note.ABI-tag") == 0 || strcmp(name, ".comment") == 0) {
            fseek(f, shdr.sh_offset, SEEK_SET);
            char junk[] = "RANDOMIZED-SECTION\0";
            fwrite(junk, 1, sizeof(junk) - 1, f);
            break;
        }
    }
    fclose(f);
}

// Dynamically resolve libc function
void use_dlopen_entropy() {
    void *handle = dlopen("libc.so.6", RTLD_LAZY);
    if (!handle) return;

    int (*puts_fn)(const char *) = dlsym(handle, "puts");
    if (puts_fn) {
        puts_fn("dlopen says hi");
    }

    dlclose(handle);
}

int main() {
    unsigned int seed;
    get_entropy((unsigned char*)&seed, sizeof(seed));
    srand(seed ^ __rdtsc());

    polymorphic_junk();
    junk_memory();
    patch_note_section();
    use_dlopen_entropy();

    char input[MAX_LEN] = "hello";
    char mutated[MAX_LEN];
    strcpy(mutated, input);

    printf("Original: %s\n", input);

    void (*mutators[3])(char *) = { mutate1, mutate2, mutate3 };
    shuffle(mutators, 3);
    for (int i = 0; i < 3; ++i)
        mutators[i](mutated);

    printf("Mutated : %s\n", mutated);

    int val = run_jit_code();
    printf("JIT Code Result: %d\n", val);
    return 0;
}
