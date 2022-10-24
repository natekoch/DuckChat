#include "duckchat.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    
    char *hostname, *username;
    long port;

    if (argc == 4) {
        hostname = malloc(strlen(argv[1]) + 1);
        strcpy(hostname, argv[1]);

        port = atol(argv[2]);

        username = malloc(strlen(argv[3]) + 1);
        strcpy(hostname, argv[3]);
    }
    else {
        printf("Usage: ./client server_socket server_port username\n");
        goto exit;
    }

exit:
    if (hostname != NULL) free(hostname);
    if (username != NULL) free(username);

    return EXIT_SUCCESS;

}
