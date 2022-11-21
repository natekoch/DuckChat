/* 
 * server.c
 *
 * Nate Koch
 * UO CS 432
 *
 * usage: ./server <hostname> <port>
 *
 * resources: https://beej.us/guide/bgnet/html/
 */

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

// store command line inputs
char HOSTNAME[UNIX_PATH_MAX];
int PORT;

int sockfd = 0;

// keep track of the users
user *clients; 
int user_count = 0;

// keep track of the channels
channel *channels; 
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

    // catch the hostuser closing the server
    signal(SIGINT, exit_handler);
    atexit(free_all);

    // Initialize clients array
    init_clients();
    // Initialize channels array
    init_channels();

    // create the socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error: could not create socket.\n");
    }

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr); 
    memset((char *)&client_addr, 0, sizeof(client_addr));

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

    int ret;

    while (1) {

        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);

        select((sockfd+1), &read_fds, NULL, NULL, NULL);

        // receive the packet from a client
        memset(client_buffer, 0, sizeof(client_buffer));
        ret = recvfrom(sockfd, client_buffer, sizeof(client_buffer), 0, 
                (struct sockaddr *)&client_addr, 
                &client_len);

        if (ret == 0) { // this doesn't work but doesn't do any harm
            printf("Client has disconnected.\n");
        }

        strcpy(client_domain, inet_ntoa(client_addr.sin_addr));
        sprintf(client_port, "%d", ntohs(client_addr.sin_port));
       
        recv_packet = (struct request *) client_buffer;

        // check if client is known
        if (lookup_client(client_domain, client_port) != -1) {
            // client is known
            if (recv_packet->req_type == REQ_LOGIN) {
                printf("Logged in user tried to login again. Ignoring.\n");
            } else if (recv_packet->req_type == REQ_LOGOUT) {
                logout_user(client_domain, client_port);
            } else if (recv_packet->req_type == REQ_JOIN) {
                if (user_join(client_domain, client_port, recv_packet) == -1) {
                    send_error(client_domain, client_port, 
                                "Error: Requested channel is full.");
                }
            } else if (recv_packet->req_type == REQ_LEAVE) {
                if (user_leave(client_domain, client_port, recv_packet) == -1) {
                    send_error(client_domain, client_port, 
                        "Error: User not subscribed or channel doesn't exist.");
                }
            } else if (recv_packet->req_type == REQ_SAY) {
                if (user_say(client_domain, client_port, recv_packet) == -1) {
                    send_error(client_domain, client_port, 
                        "Error: User tried to speak without first joining.");
                }
            } else if (recv_packet->req_type == REQ_LIST) {
                user_list(client_domain, client_port);
            } else if (recv_packet->req_type == REQ_WHO) {
                if (user_who(client_domain, client_port, recv_packet) == -1) {
                    send_error(client_domain, client_port, 
                                "Error: Channel does not exist.");
                }
            } else {
                printf("Received bogus packet. Ignoring.\n");
            }
        } else { 
            // client is unknown
            if (recv_packet->req_type == REQ_LOGIN) {
                // client wants to login
                if (user_count != 20) {
                    login_user(client_domain, client_port, recv_packet, 
                                &client_addr);
                } else {
                    printf("Server is full.\n");
                    send_error(client_domain, client_port, 
                                "Error: Server is full.");
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

// catch the host closing the server to have graceful exit.
static void exit_handler(int signum) {
    (void) signum;
    exit(EXIT_SUCCESS);
}

// socket and memory cleanup. 
static void free_all() {
    close(sockfd);
    if (channels != NULL)
        free_channels();
    if (clients != NULL)
        free_clients();
}

// initialize the channels list
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

// initialize the clients list
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

// free the channels list
static void free_channels() {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        free(channels[i].name);
        free(channels[i].user_indecies);
    }
    free(channels);
}

// free the clients list
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

// look up a client and give the index to their spot in the clients list
// return -1 if they aren't a logged in client
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

// look up a channel by its name and give the index to its spot in the channels
// list return -1 if the channel doesn't exist
static int lookup_channel(char *channel_name) {
    int ret = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].name, channel_name) == 0) {
            ret = i;
            break;
        }
    }

    return ret;
}

// login the new user
static void login_user(char *client_domain, char *client_port, 
                        struct request *recv_packet, 
                        struct sockaddr_in *client_addr) {
    struct request_login *login = (struct request_login *) recv_packet;
    for (int i = 0; i < MAX_USERS; i++) {
        if (strcmp(clients[i].name, "") == 0) {
            strncpy(clients[i].name, login->req_username, USERNAME_MAX);
            strncpy(clients[i].domain, client_domain, UNIX_PATH_MAX);
            strncpy(clients[i].port, client_port, 6);
            *(clients[i].address) = *client_addr;
            break;
        }
    }
    user_count++;
    printf("%s has logged in.\n", login->req_username);
}

// logout the logged in user and remove them from their channels
static void logout_user(char *client_domain, char *client_port) {
    // remove user from clients
    int i = lookup_client(client_domain, client_port);

    // remove user from all associated channels
    for (int j = 0; j < MAX_CHANNELS; j++) {
        for (int k = 0; k < channels[j].nusers; k++) {
            if (channels[j].user_indecies[k] == i) {
                channels[j].user_indecies[k] = -1;
                channels[j].nusers--;
                printf("%s has now left %s.\n", clients[i].name, 
                        channels[j].name);
                if (channels[j].nusers == 0) {
                    // remove empty channel
                    printf("%s now empty. Removing the channel.\n", 
                            channels[j].name);
                    strcpy(channels[j].name, "");
                    channel_count--;
                }
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

// handle the user requesting to join the specified channel
static int user_join(char *client_domain, char *client_port, 
                        struct request *recv_packet) {
    int ret = -1;
    int open_spot = -1;

    struct request_join *join = (struct request_join *) recv_packet;

    // look up channel does it exist?
    int j = lookup_channel(join->req_channel);

    // look up user
    int i = lookup_client(client_domain, client_port);

    if (j != -1) { 

        // check if user already in channel
        for (int k = 0; k < MAX_CHANNELS; k++) {
            if (channels[j].user_indecies[k] == i) {
                ret = 1;
                goto end;
            }
            if (channels[j].user_indecies[k] == -1 && open_spot == -1) {
                open_spot = k;
            }
        }
        // add user to existing channel
        if (channels[j].nusers != MAX_USERS) {
            // use the found empty spot
            channels[j].user_indecies[open_spot] = i;
            channels[j].nusers++;
        } else {
            goto end; // channel list was full
        }
    } else {
        // no create channel and add user and update their channels
        for (int k = 0; k < MAX_CHANNELS; k++) {
            if (strcmp(channels[k].name, "") == 0) {
                strncpy(channels[k].name, join->req_channel, CHANNEL_MAX);
                channels[k].nusers++;
                channels[k].user_indecies[0] = i;
                channel_count++;
                break;
            }
        }
        printf("%s has been created by %s.\n", join->req_channel, 
                                                            clients[i].name);
    }

    // add channel to user's list
    for (int k = 0; k < MAX_CHANNELS; k++) {
        if (strcmp(clients[i].channels[k], "") == 0) {
            strcpy(clients[i].channels[k], join->req_channel);
            break;
        }
    }
    ret = 1;
    printf("%s has joined %s.\n", clients[i].name, join->req_channel);

    // ret -1 if channel list is full 
end:
    return ret; 
}

// handle the user requesting to leave the specified channel
static int user_leave(char *client_domain, char *client_port, 
                                                struct request *recv_packet) {
    int ret = -1;
    
    struct request_leave *leave = (struct request_leave *) recv_packet;

    // look up user
    int i = lookup_client(client_domain, client_port);

    // look up channel to see if it exists
    int j = lookup_channel(leave->req_channel);
    if (j != -1) {
        // remove user from channel
        for (int k = 0; k < MAX_CHANNELS; k++) {
            if (channels[j].user_indecies[k] == i) {
                channels[j].user_indecies[k] = -1;
                channels[j].nusers--;
                printf("%s has now left %s.\n", clients[i].name, 
                        channels[j].name);
                if (channels[j].nusers == 0) {
                    // remove empty channel
                    printf("%s now empty. Removing the channel.\n", 
                            leave->req_channel);
                    strcpy(channels[j].name, "");
                    channel_count--;
                }
                break;
            }
        }
        // remove channel from user
        for (int k = 0; k < MAX_CHANNELS; k++) {
            if (strcmp(clients[i].channels[k], leave->req_channel) == 0) {
                strcpy(clients[i].channels[k], "");
                break;
            }
        }
        ret = 1;
    }

    // ret -1 if the channel doesn't exist or 
    // the user doesn't belong to the channel.
    return ret; 
}

// handle the user sending a message in the specified channel and send the text
// of their message to all the users in that channel
static int user_say(char *client_domain, char *client_port, 
                                                struct request *recv_packet) {
    int can_speak = -1;

    struct request_say *say = (struct request_say *) recv_packet;

    struct text_say send_say;
    memset(&send_say, 0, sizeof(send_say));

    // lookup user
    int i = lookup_client(client_domain, client_port);

    // lookup channel
    int j = lookup_channel(say->req_channel);
    if (j != -1) {
        // is user in requested channel
        for (int k = 0; k < MAX_USERS; k++) {
            if (channels[j].user_indecies[k] == i) {
                can_speak = 1;
                break;
            }
        }

        if (can_speak) {
            // user is in requested channel can speak
            // for all users in channel send them the message
            for (int k = 0; k < MAX_USERS; k++) {
                if (channels[j].user_indecies[k] != -1) {
                    send_say.txt_type = TXT_SAY;
                    strncpy(send_say.txt_username, clients[i].name, 
                            USERNAME_MAX);
                    strncpy(send_say.txt_channel, say->req_channel, 
                            CHANNEL_MAX);
                    strncpy(send_say.txt_text, say->req_text, SAY_MAX);
                    
                    sendto(sockfd, &send_say, sizeof(send_say), 0, 
                            (struct sockaddr *) 
                            clients[channels[j].user_indecies[k]].address, 
                        sizeof(*clients[channels[j].user_indecies[k]].address));
                }
            }
            printf("%s sent a message in %s.\n", clients[i].name, say->req_channel);
        }
        // if user is not in requested channel cannot speak ret -1 
    }

    return can_speak;
}

// handle the user requesting to get a list of all the channels on the server
static void user_list(char *client_domain, char *client_port) {
    struct text_list send_list;
    memset(&send_list, 0, sizeof(send_list));

    int i = lookup_client(client_domain, client_port);
    
    send_list.txt_type = TXT_LIST;
    send_list.txt_nchannels = channel_count;

    int l = 0;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (strcmp(channels[i].name, "") != 0) {
            strncpy(send_list.txt_channels[l].ch_channel, 
                    channels[i].name, 
                    CHANNEL_MAX);            
            l++;
        }
    }

    sendto(sockfd, &send_list, sizeof(send_list), 0, 
            (struct sockaddr *) clients[i].address, 
            sizeof(*clients[i].address));

    printf("%s requested the list of channels.\n", clients[i].name);
}

// handle the user requesting to have a list of all the users on the 
// specified channels
static int user_who(char *client_domain, char *client_port, 
                                            struct request *recv_packet) {
    int ret = -1;

    struct request_who *who = (struct request_who *) recv_packet;

    struct text_who send_who;
    memset(&send_who, 0, sizeof(send_who));
    
    int i = lookup_client(client_domain, client_port);

    int j = lookup_channel(who->req_channel);

    if (j != -1) {
        send_who.txt_type = TXT_WHO;
        send_who.txt_nusernames = channels[j].nusers;
        strncpy(send_who.txt_channel, who->req_channel, CHANNEL_MAX);
        int l = 0;
        for (int k = 0; k < MAX_USERS; k++) {
            if (channels[j].user_indecies[k] != -1) {
                strncpy(send_who.txt_users[l].us_username, 
                        clients[channels[j].user_indecies[k]].name, 
                        USERNAME_MAX);
                l++;
            }
        }
        
        sendto(sockfd, &send_who, sizeof(send_who), 0, 
            (struct sockaddr *) clients[i].address, 
            sizeof(*clients[i].address));
        
        ret = 1;

        printf("%s asked who was in %s.\n", clients[i].name, who->req_channel);
    }

    // return -1 if channel doesn't exist
    return ret;
}

// send an error message to the user that caused the erro
static void send_error(char *client_domain, char *client_port, char *message) {
    int i = lookup_client(client_domain, client_port);
    struct text_error error;
    memset(&error, 0, sizeof(error));
    
    error.txt_type = TXT_ERROR;
    strncpy(error.txt_error, message, SAY_MAX);

    sendto(sockfd, &error, sizeof(error), 0, 
            (struct sockaddr *) clients[i].address, 
            sizeof(*clients[i].address));

    printf("Sent error message to %s.\n", clients[i].name);
}
