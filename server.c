#include "server.h"
#include "duckchat.h"
#include <netinet/in.h>
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

user clients[20];

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

    if ((listener = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error: could not create socket.\n");
    }

    struct hostent *host;
    host = gethostbyname(HOSTNAME); 
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = *(in_addr_t *) host->h_addr_list[0];
    
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
        sprintf(client_port, "%d", ntohs(client_addr.sin_port));
       
        printf("%s:%s\n", client_domain, client_port);

        recv_packet = (struct text *) client_buffer;

        printf("%d\n", recv_packet->txt_type);
    }

exit:
    exit(EXIT_SUCCESS);
}
/*
static int add_user(char *client_domain, char *client_port) {
    int ret = 0;
    // TODO: FINISH

    // search for an empty spot in clients
    // place new user into empty spot ret 1
    // if no room ret 0 

    return ret;
}
*/
