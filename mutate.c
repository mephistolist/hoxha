#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <x86intrin.h>
#include <elf.h>
#include <errno.h>
#include <pthread.h>

#define MAX_LEN 128
#define PATH_MAX 200

static unsigned long long internal_seed = 0;

// Add this at the top for FreeBSD/Linux self path logic
#ifdef __FreeBSD__
    const char *self = "/proc/curproc/file";
#else
    const char *self = "/proc/self/exe";
#endif

// Prototype
void mutate1(char *s);
void junk_memory(void);
unsigned char internal_random_byte(void);
void shuffle(void (**funcs)(char *), int count);

void *background_entropy() {
    while (1) {
        junk_memory();
        usleep(100000 + (internal_random_byte() % 100000));
    }
    return NULL;
}

void execute_mutations(char *s, void (**mutators)(char *), int count) {
    void (*dispatch[6])(char *) = {0};
    for (int i = 0; i < count; ++i) {
        dispatch[i] = mutators[i];
    }
    shuffle(dispatch, count);
    for (int i = 0; i < count; ++i) {
        dispatch[i](s);
    }
}

// Insert this new function before main()
void patch_mutate1() {
    unsigned char *target = (unsigned char *)(uintptr_t)mutate1;    

    uintptr_t page_start = (uintptr_t)target & ~(sysconf(_SC_PAGE_SIZE) - 1);
    mprotect((void *)page_start, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_WRITE | PROT_EXEC);

    // Overwrite mutate1 with NOPs
    for (int i = 0; i < 16; ++i) {
        target[i] = 0x90; // NOP
    }

    // Replace first instruction with RET (makes mutate1 a no-op)
    target[0] = 0xC3;

    // Restore page protection (optional but cleaner)
    mprotect((void *)page_start, sysconf(_SC_PAGE_SIZE), PROT_READ | PROT_EXEC);
}

void init_entropy() {
    unsigned long long t = __rdtsc();
    unsigned long long a = (unsigned long long)&t;
    unsigned long long b = (unsigned long long)time(NULL);
    unsigned long long c = (unsigned long long)getpid();
    internal_seed = (t ^ a ^ b ^ c) * 0x5DEECE66DULL + 0xB;
}

unsigned char internal_random_byte() {
    internal_seed ^= internal_seed >> 21;
    internal_seed ^= internal_seed << 35;
    internal_seed ^= internal_seed >> 4;
    internal_seed *= 2685821657736338717ULL;
    return (unsigned char)(internal_seed >> 56);
}

void get_entropy(unsigned char *buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buf[i] = internal_random_byte();
    }
}

// Mutation Functions (unchanged)
void mutate1(char *s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len / 2; ++i) {
        char tmp = s[i];
        s[i] = s[len - i - 1];
        s[len - i - 1] = tmp;
    }
}

void mutate2(char *s) {
    for (size_t i = 0; s[i]; ++i) {
        if ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z')) {
            s[i]++;
	}
    }
}

void mutate3(char *s) {
    unsigned char key[1];
    get_entropy(key, 1);
    for (size_t i = 0; s[i]; ++i) {
        s[i] ^= key[0];
    }
}

void mutate4(char *s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len; i++) {
        if (i % 2 == 0)
            s[i] = (s[i] ^ internal_random_byte()) & 0x7F;
    }
}

void mutate5(char *s) {
    unsigned char junk = internal_random_byte() % 94 + 33;
    size_t len = strlen(s);
    if (len > 1) {
        s[internal_random_byte() % len] = junk;
    }
}

void mutate6(char *s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len; i++) {
        if (internal_random_byte() % 3 == 0) {
            s[i] = s[i] ^ (char)(internal_random_byte() % 64);
	}
    }
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
    if (mem == MAP_FAILED) { return -1; }

    // Fill memory with entropy (garbage-instruction polymorphism)
    for (size_t i = 0; i < pagesize; ++i) {
        mem[i] = internal_random_byte();
    }

    // Optionally add polymorphic junk before real code
    int pre_nops = internal_random_byte() % 8;
    size_t offset = 0;
    for (int i = 0; i < pre_nops; ++i) {
        mem[offset++] = 0x90; // NOP
    }

    // Generate a random return value
    unsigned char random_val;
    get_entropy(&random_val, 1);

    // Emit x86_64 function returning random_val
    unsigned char program[] = {
        0x55,                   // push   %rbp
        0x48, 0x89, 0xe5,       // mov    %rsp,%rbp
        0xb8, random_val, 0x00, 0x00, 0x00, // mov $val,%eax
        0x5d,                   // pop    %rbp
        0xc3                    // ret
    };

    memcpy(mem + offset, program, sizeof(program));
    offset += sizeof(program);

    // Add NOPs after the function (junk padding)
    int post_nops = internal_random_byte() % 8;
    for (int i = 0; i < post_nops; ++i) {
        mem[offset++] = 0x90;
    }

    // Optional debug output
    if (getenv("SHOW_JIT_BYTES")) {
        printf("JIT machine code (%zu bytes):\n", offset);
        for (size_t i = 0; i < offset; ++i) {
            printf("%02x ", mem[i]);
	}
        printf("\n");
    }

    // Execute the code
    int (*func)() = (int (*)())(uintptr_t)mem;
    int result = func();

    // Overwrite memory with noise before optional unmapping
    for (size_t i = 0; i < offset; ++i) {
        mem[i] = internal_random_byte();
    }

    // Unmap to clean up
    munmap(mem, pagesize);
    return result;
}

void junk_memory() {
    size_t allocs = 5 + (internal_random_byte() % 5);
    for (size_t i = 0; i < allocs; ++i) {
        size_t size = 512 + (internal_random_byte() % 1024);
        char *buf = malloc(size);
        if (!buf) { continue; }
        for (size_t j = 0; j < size; ++j) {
            buf[j] = internal_random_byte();
	}
        volatile char dummy = buf[internal_random_byte() % size];
        (void)dummy;
        free(buf);
    }
}

void polymorphic_junk() {
    unsigned char r = internal_random_byte();
    for (int i = 0; i < (r % 5) + 1; ++i) {
        asm volatile (
            "nop\n\t"
            "xor %%eax, %%eax\n\t"
            "add $0x1, %%eax\n\t"
            :
            :
            : "eax"
        );
    }
}

void patch_note_section(FILE *f) {
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
}

int mutate_main(int argc, char **argv) {
    (void)argc; 
    init_entropy();
    srand(internal_seed ^ __rdtsc());

    // Start background entropy thread
    pthread_t tid;
    pthread_create(&tid, NULL, background_entropy, NULL);

    polymorphic_junk();
    junk_memory();
    //patch_note_section();
    // === Try to open executable using realpath(argv[0]) first ===
    FILE *f = NULL;
    char resolved_path[PATH_MAX];

    if (realpath(argv[0], resolved_path)) {
        f = fopen(resolved_path, "r+b");
    }

    // === Fallback: /proc/self/exe ===
    if (!f) {
        f = fopen("/proc/self/exe", "r+b");
        if (f) {
            unlink("/proc/self/exe"); // cleanup handle
        }
    }

    if (f) {
        patch_note_section(f);
        fclose(f);
    }

    patch_mutate1();

    char input[MAX_LEN];
    unsigned char len_byte;
    get_entropy(&len_byte, 1);
    int len = 5 + (len_byte % (MAX_LEN - 6));
    for (int i = 0; i < len; ++i) {
        unsigned char c;
        get_entropy(&c, 1);
        input[i] = 33 + (c % 94); // printable ASCII
    }
    input[len] = '\0';

    char mutated[MAX_LEN];
    strcpy(mutated, input);
    //printf("Original: %s\n", input);

    void (*mutators[])(char *) = {
        mutate1, mutate2, mutate3, mutate4, mutate5, mutate6
    };
    int mut_count = sizeof(mutators) / sizeof(mutators[0]);
    shuffle(mutators, mut_count);

    unsigned char passes = 2 + (internal_random_byte() % 4); // 2 to 5 passes
    for (int i = 0; i < passes; ++i) {
        mutators[i % mut_count](mutated);
    }
    //printf("Mutated : %s\n", mutated);

    int val = run_jit_code();
    (void)val;
    //printf("JIT Code Result: %d\n", val);
    return 0;
}
