#include "server.h"
#include "duckchat.h"
#include <netinet/in.h>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

char HOSTNAME[UNIX_PATH_MAX];
int PORT;

int main(int argc, char *argv[]) {

    // handle initial server input
    if (argc == 3) {
        if (strlen(argv[1]) > UNIX_PATH_MAX) {
            printf("Error: hostname is too long");
            goto exit;
        }
        strcpy(HOSTNAME, argv[1]);

        char* port_fail;
        PORT = strtol(argv[2], &port_fail, 10);
        if (port_fail == argv[2] || *port_fail != '\0') {
            printf("Invalid port number.\n");
            goto exit;
        }
    } else {
        printf("Usage: ./server hostname port\n");
        goto exit;
    }

    int listener = 0;
    struct sockaddr_in server_addr, client_addr;

    if (listener = socket(AF_INET, SOCK_DGRAM, 0) < 0) {
        printf("Error: could not create socket.\n");
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htnos(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    
    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: could not bind server and socket.\n");
    }

    fd_set read_fds;

    struct text *recv_packet;

    char client_buffer[1024];

    char client_domain[UNIX_PATH_MAX];
    char client_port[5];

    while (1) {

        FD_ZERO(&read_fds);
        FD_SET(listener, &read_fds);

        select((listener+1), &read_fds, NULL, NULL, NULL);

        memset(client_buffer, 0, sizeof(client_buffer));
        recvfrom(listener, client_buffer, sizeof(client_buffer), 0, 
                (struct sockaddr *)&client_addr, 
                (socklen_t *) sizeof(client_addr));
        
        strcpy(client_domain, inet_ntoa(client_addr.sin_addr));
        strcpy(client_port, ntohs(client_addr.sin_port));
        
        recv_packet = (struct text *) client_buffer;

        printf("%d\n", recv_packet->txt_type);
    }
    
    /*
    int fdmax;

    struct addrinfo hints, *res, *temp;
    
    FD_ZERO(&main_fds);
    FD_ZERO(&read_fds);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rev = getaddrinfo(NULL, argv[2], &hints, &res)) != 0) {
        printf("Error getting address information.\n");
        goto exit;
    }

    for (temp = res; temp != NULL; temp = temp->ai_next) {
        listener = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol);
        if (listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, temp->ai_addr, temp->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    if (temp == NULL) {
        printf("Failed to bind.");
        goto exit;
    }

    freeaddrinfo(res);

    FD_SET(listener, &main_fds);

    fdmax = listener;

    while(1) {
        read_fds = main_fds;
        select(fdmax+1, &read_fds, NULL, NULL, NULL);

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == listener) {
                    addrlen = sizeof(clientaddr);
                    latest_fd = accept( listener, 
                                        (struct sockaddr *)&clientaddr, 
                                        &addrlen);
                    if (latest_fd == -1) {
                        printf("Failed to accept new connection.\n");
                    } else {
                        FD_SET(latest_fd, &main_fds);
                        if (latest_fd > fdmax) {
                            fdmax = latest_fd;
                        }
                        printf("New connection\n");
                    }
                } else {
                    printf("data\n");
                }
            }
        }
    }
    */

exit:
    exit(EXIT_SUCCESS);
}
