#include "duckchat.h"
#include "raw.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    
    char HOSTNAME[UNIX_PATH_MAX]; 
    char USERNAME[USERNAME_MAX];
    int PORT;
    char current_channel[CHANNEL_MAX];

    if (argc == 4) {
        if (strlen(argv[1]) > UNIX_PATH_MAX) {
            printf("Hostname must be less than 32 bytes long.");
            goto exit;
        }
        //HOSTNAME = malloc(strlen(argv[1]) + 1);
        strcpy(HOSTNAME, argv[1]);

        PORT = atoi(argv[2]);

        if (strlen(argv[3]) > USERNAME_MAX) {
            // TODO: Figure out the expected client response from test binaries
            printf("Username must be less than 32 bytes long.");
            goto exit;
        }
        //USERNAME = malloc(strlen(argv[3]) + 1);
        strcpy(USERNAME, argv[3]);
    }
    else {
        printf("Usage: ./client server_socket server_port username\n");
        goto exit;
    }

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
    strcpy(current_channel, "Common");

    struct request_join join_init;
    join_init.req_type = REQ_JOIN;
    strcpy(join_init.req_channel, current_channel);

    send(sockfd, &join_init, sizeof(join_init), 0);

    // listen stdin for chat and commands
    while (1) {
        char input_buf[SAY_MAX];
        char c;
        printf(">");
        raw_mode();
        int i = 0;
        while (1) {
            c = fgetc(stdin);
            if (c == '\n') break;
            if (i < SAY_MAX) {
                input_buf[i] = c;
                printf("%c", c);
            }
            input_buf[i+1] = '\0';
            i++;
        }
        printf("\n");
        //printf("%s\n", input_buf);

        if (input_buf[0] == '/') { // parse command
            char command[7];
            char channel[CHANNEL_MAX];
            sscanf(input_buf, "%7s %32s", command, channel);
            if (strncmp(command, "/exit", 5) == 0) { 
                // logout of server
                struct request_logout logout;
                logout.req_type = REQ_LOGOUT;

                send(sockfd, &logout, sizeof(logout), 0);

                goto exit;

            } else if (strncmp(command, "/join", 5) == 0) {
                if (input_buf[5] == ' ') {
                    // join channel
                    struct request_join join;
                    join.req_type = REQ_JOIN;
                    strcpy(join.req_channel, channel);
                    
                    send(sockfd, &join, sizeof(join), 0);

                    // TODO: update user channels list
                } else printf("Invalid usage: /join channel\n");
            } else if (strncmp(command, "/leave", 6) == 0) {
                if (input_buf[6] == ' ') {
                    // leave channel 
                    struct request_leave leave;
                    leave.req_type = REQ_LEAVE;
                    strcpy(leave.req_channel, channel);

                    send(sockfd, &leave, sizeof(leave), 0);

                    // TODO: update user channels list
                } else printf("Invalid usage: /leave channel\n");
            } else if (strncmp(command, "/list", 5) == 0) {
                // list channels
                struct request_list list;
                list.req_type = REQ_LIST;

                send(sockfd, &list, sizeof(list), 0);
            } else if (strncmp(command, "/who", 4) == 0) {
                if (input_buf[4] == ' ') {    
                    // list users in channel
                    struct request_who who;
                    who.req_type = REQ_WHO;
                    strcpy(who.req_channel, channel);

                    send(sockfd, &who, sizeof(who), 0);
                } else printf("Invalid usage: /who channel\n"); 
            } else if (strncmp(command, "/switch", 7) == 0) {
                if (input_buf[7] == ' ') {
                    printf("s\n");
                    // TODO: switch to channel
                    // need to keep list of channels 
                    // give error if not in a channel
                    // change current_channel variable
                } else printf("Invalid usage: /switch channel\n");
            } else {
                printf("*Unknown command\n");
            }
        } else { // send say message
            struct request_say msg;
            msg.req_type = REQ_SAY;
            strcpy(msg.req_channel, current_channel);
            strcpy(msg.req_text, input_buf);

            send(sockfd, &msg, sizeof(msg), 0);
        }

        struct text recv_text;
        recv(sockfd, &recv_text, 1024, 0);
        printf("%d", recv_text.txt_type);
    }
    
    // print out any responses from server

exit:
    cooked_mode();
    exit(EXIT_SUCCESS);

}
