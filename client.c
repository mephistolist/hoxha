#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#define PORT 5000
#define BUFSIZE 4096

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in servaddr;
    char buffer[BUFSIZE];

    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    while (1) {
        printf("sctp-shell> ");
        fflush(stdout);

        memset(buffer, 0, BUFSIZE);
        if (!fgets(buffer, BUFSIZE, stdin)) break;
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline

        // Check for local exit command
        if (strcmp(buffer, "exit") == 0) {
            printf("[*] Exiting client.\n");
            break;
        }

        sctp_sendmsg(sock, buffer, strlen(buffer), NULL, 0, 0, 0, 0, 0, 0);

        // Read server response
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
