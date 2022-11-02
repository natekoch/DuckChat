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
#include <signal.h>

char HOSTNAME[UNIX_PATH_MAX];
int PORT;

user *clients;

channel *channels;

int user_count = 0;
int channel_count = 0;

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

    signal(SIGINT, exit_handler);
    atexit(free_all);

    // Initialize clients array
    init_clients();
    // Initialize channels array
    init_channels();

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

    struct request *recv_packet;

    char client_buffer[1024];

    char client_domain[UNIX_PATH_MAX];
    char client_port[5];

    int user_i;

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

        recv_packet = (struct request *) client_buffer;

        // check if client is known
        if ((user_i = lookup_client(client_domain, client_port)) != -1) {
            // client is known
            if (recv_packet->req_type == REQ_LOGIN) {
                printf("Logged in user tried to login again. Ignoring.\n");
            } else if (recv_packet->req_type == REQ_LOGOUT) {
                logout_user(client_domain, client_port);
            } else if (recv_packet->req_type == REQ_JOIN) {

            } else if (recv_packet->req_type == REQ_LEAVE) {

            } else if (recv_packet->req_type == REQ_SAY) {

            } else if (recv_packet->req_type == REQ_LIST) {

            } else if (recv_packet->req_type == REQ_WHO) {

            } else {
                printf("Received bogus packet. Ignoring.\n");
            }
        } else { 
            // client is unknown
            if (recv_packet->req_type == REQ_LOGIN) {
                // client wants to login
                if (user_count != 20) {
                    login_user(client_domain, client_port, recv_packet);
                } else {
                    printf("Server is full.\n");
                    // TODO: send error message
                }
            } else {
                // client is unknown and not requesting to login just ignore
                printf("Not logged in client tried to send a packet.\n");
            }
        }
    }

exit:
    exit(EXIT_FAILURE);
}

static void exit_handler(int signum) {
    (void) signum;
    exit(EXIT_SUCCESS);
}

static void free_all() {
    if (channels != NULL)
        free_channels();
    if (clients != NULL)
        free_clients();
}

static void init_channels() {
    // init the channels array of channel elements
    channels = malloc(sizeof(channel) * MAX_CHANNELS);
    for (int i = 0; i < MAX_CHANNELS; i++) {
        channels[i].nusers = 0;
        channels[i].name = malloc(CHANNEL_MAX);
        strcpy(channels[i].name, "");
        channels[i].users = malloc(sizeof(char *) * MAX_USERS);
        for (int j = 0; j < MAX_USERS; j++) {
            channels[i].users[j] = malloc(USERNAME_MAX);
            strcpy(channels[i].users[j], "");
        }
    }
}

static void init_clients() {
    // init the clients array of user elements
    clients = malloc(sizeof(user) * MAX_USERS);
    for (int i = 0; i < MAX_USERS; i++) {
        clients[i].name = malloc(USERNAME_MAX);
        strcpy(clients[i].name, "");
        clients[i].domain = malloc(UNIX_PATH_MAX);
        strcpy(clients[i].domain, "");
        clients[i].port = malloc(5);
        strcpy(clients[i].port, "");
        clients[i].channels = malloc(sizeof(char *) * MAX_CHANNELS);
        for (int j = 0; j < MAX_CHANNELS; j++) {
            clients[i].channels[j] = malloc(CHANNEL_MAX);
            strcpy(clients[i].channels[j], "");
        }
    }
}

static void free_channels() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        for (int j = 0; j < MAX_USERS; j++) {
            free(channels[i].users[j]);
        }
        free(channels[i].name);
        free(channels[i].users);
    }
    free(channels);
}

static void free_clients() {
    for (int i = 0; i < MAX_USERS; i++) {
        for (int j = 0; j < MAX_CHANNELS; j++) {
            free(clients[i].channels[j]);
        }
        free(clients[i].name);
        free(clients[i].domain);
        free(clients[i].port);
        free(clients[i].channels);
    }
    free(clients);
}

static int lookup_client(char *client_domain, char *client_port) {
    int ret = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if ((strcmp(clients[i].domain, client_domain) == 0) 
            && (strcmp(clients[i].port, client_port) == 0)) {
            ret = i;
            break;        
        }
    }
    return ret;
}

static void login_user(char *client_domain, char *client_port, struct request *recv_packet) {
    struct request_login *login = (struct request_login *) recv_packet;
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(clients[i].name, "") == 0) {
            strcpy(clients[i].name, login->req_username);
            strcpy(clients[i].domain, client_domain);
            strcpy(clients[i].port, client_port);
        }
    }
    user_count++;
    printf("%s has logged in.\n", login->req_username);
}

static void logout_user(char *client_domain, char *client_port) {
    // remove user from clients
    int i = lookup_client(client_domain, client_port);

    // remove user from all associated channels
    for (int j = 0; j < MAX_CHANNELS; j++) {
        for (int k = 0; k < channels[j].nusers; k++) {
            if (strcmp(channels[j].users[k], clients[i].name) == 0) {
                strcpy(channels[j].users[k], "");
                channels[j].nusers--;
                break;
            }
        }
    }
    
    printf("%s has logged out.\n", clients[i].name);

    // Clear clients[i] entry
    strcpy(clients[i].name, "");
    strcpy(clients[i].domain, "");
    strcpy(clients[i].port, "");
    for (int p = 0; p < MAX_CHANNELS; p++) {
        strcpy(clients[i].channels[p], "");
    }
    user_count--;
}
