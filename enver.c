#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>

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

int sock = -1;  // Global socket descriptor for signal handler

void cleanup() {
    if (sock != -1) {
        printf("\n[*] Closing SCTP connection...\n");
        close(sock);
        sock = -1;
    }
    exit(0);
}

void handle_signal(int sig) {
    printf("\n[*] Caught signal %d, cleaning up...\n", sig);
    cleanup();
}

void send_knock_sequence_sctp(const char *ip) {
    struct sockaddr_in servaddr;

    for (int i = 0; i < SEQ_LEN; i++) {
        int tmp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        if (tmp_sock < 0) {
            perror("SCTP socket");
            exit(1);
        }

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(KNOCK_PORT);
        servaddr.sin_addr.s_addr = inet_addr(ip);

        if (connect(tmp_sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
            perror("SCTP knock connect");
            close(tmp_sock);
            exit(1);
        }

        if (sctp_sendmsg(tmp_sock, KNOCK_SEQUENCE[i], strlen(KNOCK_SEQUENCE[i]),
                         NULL, 0, 0, 0, 0, 0, 0) < 0) {
            perror("SCTP send knock");
            close(tmp_sock);
            exit(1);
        }

        close(tmp_sock);
        sleep(INTERVALS[i]);
    }

    printf("[+] SCTP knock sequence sent.\n");
}

int main(int argc, char *argv[]) {
    struct sockaddr_in servaddr;
    char buffer[BUFSIZE];

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGQUIT, handle_signal);

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
        if (!fgets(buffer, BUFSIZE, stdin)) {
            // Handle EOF (Ctrl+D)
            printf("\n[*] EOF received, exiting...\n");
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "exit") == 0) {
            printf("[*] Exiting.\n");
            break;
        }

        if (sctp_sendmsg(sock, buffer, strlen(buffer), NULL, 0, 0, 0, 0, 0, 0) < 0) {
            perror("SCTP send");
            break;
        }

        while (1) {
            memset(buffer, 0, BUFSIZE);
            int n = sctp_recvmsg(sock, buffer, BUFSIZE, NULL, 0, 0, 0);
            if (n <= 0) {
                fprintf(stderr, "[!] Connection closed or error.\n");
                cleanup();
                return 1;
            }

            if (strncmp(buffer, "__END__", 7) == 0)
                break;

            write(STDOUT_FILENO, buffer, n);
        }

        printf("\n");
    }

    cleanup();
    return 0;
}
