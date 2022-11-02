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

int sockfd = 0;
struct sockaddr_in server_addr, client_addr;

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

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error: could not create socket.\n");
    }

    struct hostent *host;
    host = gethostbyname(HOSTNAME); 
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = *(in_addr_t *) host->h_addr_list[0];
    
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: could not bind server and socket.\n");
    }

    fd_set read_fds;

    struct request *recv_packet;

    char client_buffer[1024];

    char client_domain[UNIX_PATH_MAX];
    char client_port[5];

    while (1) {

        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        select((sockfd+1), &read_fds, NULL, NULL, NULL);

        memset(client_buffer, 0, sizeof(client_buffer));
        recvfrom(sockfd, client_buffer, sizeof(client_buffer), 0, 
                (struct sockaddr *)&client_addr, 
                (socklen_t *) sizeof(client_addr));
        
        strcpy(client_domain, inet_ntoa(client_addr.sin_addr));
        sprintf(client_port, "%d", ntohs(client_addr.sin_port));
       
        printf("%s:%s\n", client_domain, client_port);

        recv_packet = (struct request *) client_buffer;

        // check if client is known
        if (lookup_client(client_domain, client_port) != -1) {
            // client is known
            if (recv_packet->req_type == REQ_LOGIN) {
                printf("Logged in user tried to login again. Ignoring.\n");
            } else if (recv_packet->req_type == REQ_LOGOUT) {
                logout_user(client_domain, client_port);
            } else if (recv_packet->req_type == REQ_JOIN) {
                //user_join(client_domain, client_port, recv_packet);
            } else if (recv_packet->req_type == REQ_LEAVE) {
                //user_leave(client_domain, client_port, recv_packet);
            } else if (recv_packet->req_type == REQ_SAY) {
                //user_say(client_domain, client_port, recv_packet);
            } else if (recv_packet->req_type == REQ_LIST) {
                //user_list(client_domain, client_port, recv_packet);
            } else if (recv_packet->req_type == REQ_WHO) {
                //user_who(client_domain, client_port, recv_pakcet);
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
        channels[i].user_indecies = malloc(sizeof(int *) * MAX_USERS);
        for (int j = 0; j < MAX_USERS; j++) {
            channels[i].user_indecies[j] = -1;
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
        clients[i].port = malloc(6);
        strcpy(clients[i].port, "");
        clients[i].channels = malloc(sizeof(char *) * MAX_CHANNELS);
        for (int j = 0; j < MAX_CHANNELS; j++) {
            clients[i].channels[j] = malloc(CHANNEL_MAX);
            strcpy(clients[i].channels[j], "");
        }
        clients[i].address = malloc(sizeof(struct sockaddr_in));
    }
}

static void free_channels() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        free(channels[i].name);
        free(channels[i].user_indecies);
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
        free(clients[i].address);
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

static void login_user(char *client_domain, char *client_port, struct request *recv_packet, struct sockaddr_in *client_addr) {
    struct request_login *login = (struct request_login *) recv_packet;
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(clients[i].name, "") == 0) {
            strcpy(clients[i].name, login->req_username);
            strcpy(clients[i].domain, client_domain);
            strcpy(clients[i].port, client_port);
            clients[i].address = client_addr;
        }
    }
    user_count++;
    printf("%s has logged in.\n", login->req_username);

    // TEMP TEST
    send_error(client_domain, client_port, "Test Error.\n");

}

static void logout_user(char *client_domain, char *client_port) {
    // remove user from clients
    int i = lookup_client(client_domain, client_port);

    // remove user from all associated channels
    for (int j = 0; j < MAX_CHANNELS; j++) {
        for (int k = 0; k < channels[j].nusers; k++) {
            if (channels[j].user_indecies[k] == i) {
                channels[j].user_indecies[k] = -1;
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

static int user_join(char *client_domain, char *client_port, struct request *recv_packet) {
    // TODO
    int ret = -1;
    // look up user 

    // look up channels does it exist ?
    // yes add user to channel and update their channels

    // no create channel and add user and update their channels

    // ret -1 if channel list is full 
    return ret; 
}

static int user_leave(char *client_domain, char *client_port, struct request *recv_packet) {
    // TODO
    int ret = -1;
    // look up user

    // look up channel to see if it exists
    // remove user from channel indecies

    // remove channel from user channels list

    // ret -1 if the channel doesn't exist or 
    // the user doesn't belong to the channel.
    return ret; 
}

static void send_error(char *client_domain, char *client_port, char *message) {
    // TODO
    int i = lookup_client(client_domain, client_port);
    struct text_error error;
    error.txt_type = TXT_ERROR;
    strncpy(error.txt_error, message, SAY_MAX);

    sendto(sockfd, &error, sizeof(error), 0, (struct sockaddr *) clients[i].address, sizeof(*clients[i].address));
}