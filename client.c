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
        if (strlen(argv[1]) > UNIX_PATH_MAX)
        hostname = malloc(strlen(argv[1]) + 1);
        strcpy(hostname, argv[1]);

        port = atol(argv[2]);

        if (strlen(argv[3])  > USERNAME_MAX) {
            // TODO: Figure out the expected client response from test binaries
            printf("Username must be less than 32 bytes long.");
            goto exit;
        }
        username = malloc(strlen(argv[3]) + 1);
        strcpy(hostname, argv[3]);
    }
    else {
        printf("Usage: ./client server_socket server_port username\n");
        goto exit;
    }


    //printf("%s", username);
    // connect to server

    // listen stdin for chat and commands

    // print out any responses from server

    /*
    struct request_login *login;
    login->req_type = REQ_LOGIN;
    strcpy(login->req_username, argv);

    printf("%d %s", login->req_type, login->req_username);
    */

exit:
    if (hostname != NULL) free(hostname);
    if (username != NULL) free(username);

    return EXIT_SUCCESS;

}
