#include "duckchat.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    
    char *HOSTNAME, *USERNAME;
    int PORT;

    if (argc == 4) {
        if (strlen(argv[1]) > UNIX_PATH_MAX) {
            printf("Hostname must be less than 32 bytes long.");
            goto exit;
        }
        HOSTNAME = malloc(strlen(argv[1]) + 1);
        strcpy(HOSTNAME, argv[1]);

        PORT = atoi(argv[2]);

        if (strlen(argv[3])  > USERNAME_MAX) {
            // TODO: Figure out the expected client response from test binaries
            printf("Username must be less than 32 bytes long.");
            goto exit;
        }
        USERNAME = malloc(strlen(argv[3]) + 1);
        strcpy(USERNAME, argv[3]);
    }
    else {
        printf("Usage: ./client server_socket server_port username\n");
        goto exit;
    }

    printf("%s\n", USERNAME);
    
    // connect to server
    int sockfd = 0;
    struct sockaddr_in server_addr;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error: could not create socket.\n");
        goto exit;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // TODO: fix to have custom HOSTNAME
    
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: could not connect to server.\n");
        goto exit;
    }

    // login
    struct request_login login;// = malloc(sizeof(struct request_login));
    login.req_type = REQ_LOGIN;
    strcpy(login.req_username, USERNAME);

    send(sockfd, &login, sizeof(login), 0);
    
    // join common
    struct request_join join_init;
    join_init.req_type = REQ_JOIN;
    strcpy(join_init.req_channel, "Common");

    send(sockfd, &join_init, sizeof(join_init), 0);



    printf("%ld\n", sizeof(login));
    printf("%s\n", login.req_username);
    //free(login);
    // listen stdin for chat and commands

    // print out any responses from server

    
    

   // printf("%d %s", login->req_type, login->req_username);

exit:
    if (HOSTNAME != NULL) free(HOSTNAME);
    if (USERNAME != NULL) free(USERNAME);

    return EXIT_SUCCESS;

}
