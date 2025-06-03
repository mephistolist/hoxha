#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define KNOCK_PORT 12345
#define SCTP_PORT 5000
#define BUFSIZE 4096

#define SEQ_LEN 3
const char *KNOCK_SEQUENCE[SEQ_LEN] = {
    "nqXCT2xfFsvYktHG3d8gPV",
    "VqhEGfaeFTdSmUW7M4QkNz",
    "VXjdmp4QcBtH75S2Yf8gPx"
};
const int INTERVALS[SEQ_LEN] = {1, 2, 3};

void send_knock_sequence_sctp(const char *ip) {
    struct sockaddr_in servaddr;

    for (int i = 0; i < SEQ_LEN; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        if (sock < 0) {
            perror("SCTP socket");
            exit(1);
        }

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(KNOCK_PORT);
        servaddr.sin_addr.s_addr = inet_addr(ip);

        if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("SCTP knock connect");
            close(sock);
            exit(1);
        }

        if (sctp_sendmsg(sock, KNOCK_SEQUENCE[i], strlen(KNOCK_SEQUENCE[i]),
                         NULL, 0, 0, 0, 0, 0, 0) < 0) {
            perror("SCTP send knock");
            close(sock);
            exit(1);
        }

        close(sock);
        sleep(INTERVALS[i]);
    }

    printf("[+] SCTP knock sequence sent.\n");
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in servaddr;
    char buffer[BUFSIZE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    send_knock_sequence_sctp(argv[1]);

    // Connect to the real SCTP service
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sock < 0) {
        perror("SCTP socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SCTP_PORT);
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);

    printf("[*] Connecting to SCTP shell at %s:%d...\n", argv[1], SCTP_PORT);
    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("SCTP connect");
        close(sock);
        return 1;
    }

    printf("[+] Connected. Type commands (type 'exit' to quit):\n");

    while (1) {
        printf("sctp-shell> ");
        fflush(stdout);

        memset(buffer, 0, BUFSIZE);
        if (!fgets(buffer, BUFSIZE, stdin)) break;
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            printf("[*] Exiting.\n");
            break;
        }

        sctp_sendmsg(sock, buffer, strlen(buffer), NULL, 0, 0, 0, 0, 0, 0);

        while (1) {
            memset(buffer, 0, BUFSIZE);
            int n = sctp_recvmsg(sock, buffer, BUFSIZE, NULL, 0, 0, 0);
            if (n <= 0) {
                fprintf(stderr, "[!] Connection closed or error.\n");
                close(sock);
                return 1;
            }

            if (strncmp(buffer, "__END__", 7) == 0)
                break;

            write(STDOUT_FILENO, buffer, n);
        }

        printf("\n");
    }

    close(sock);
    return 0;
}
