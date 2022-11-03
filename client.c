#include "client.h"
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
#include <netdb.h>
#include <signal.h>

char current_channel[CHANNEL_MAX]; // keep track of current channel
char channels[20][CHANNEL_MAX]; // keep track of list of subscribed channels
int num_channels = 0; // count the number of subscribed channels

int can_speak = 0; // determines if a user can send say messages

int sockfd = 0; // client socket

// store the user command line inputs
char HOSTNAME[UNIX_PATH_MAX]; 
char USERNAME[USERNAME_MAX];
int PORT;

// store the incoming packets
char recv_buf[1024];

// flag for if raw mode was enabled
int raw = 0;

// flag for if the user has been logged in
int logged_in = 0;


int main(int argc, char *argv[]) {
    
    // handle inital client inputs
    if (argc == 4) {
        if (strlen(argv[1]) > UNIX_PATH_MAX) {
            printf("Hostname must be less than 32 bytes long.");
            goto exit;
        }
        strcpy(HOSTNAME, argv[1]);

        // TODO: Restrict port number range. 
        char* port_fail;
        PORT = strtol(argv[2], &port_fail, 10);
        if (port_fail == argv[2] || *port_fail != '\0') {
            printf("Invalid port number.\n");
            goto exit;
        }

        if (strlen(argv[3]) > USERNAME_MAX) {
            printf("Username must be less than 32 bytes long.");
            goto exit;
        }
        strcpy(USERNAME, argv[3]);
    }
    else {
        printf("Usage: ./client server_socket server_port username\n");
        goto exit;
    }

    signal(SIGINT, exit_handler);
    atexit(disconnect);

    // connect to server
    if (connect_to_server() < 0) goto exit; 
    // exit on failed socket creation or server connection
    
    // login
    send_login();

    // initialize the list of subscribed channels
    init_channels();

    // join common
    manage_channels("Common", 'a');
    send_join(current_channel);

    // fd set for select() call for sockfd and stdin
    fd_set readfds;

    // switch to raw mode for individual character input
    raw_mode();
    raw = 1;
    
    // input variables
    char input_buf[SAY_MAX];
    strcpy(input_buf, "");
    int len = 0;
    char c = '\0';

    // put the prompt > in the terminal
    putchar('>');
    fflush(stdout);

    // text structure for receving packets from the server
    struct text *recv_text; 

    // main listening loop of client input and server output
    while (1) {

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        select((sockfd + 1), &readfds, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            c = getchar();
            if (c != '\n') { 
                if (len < SAY_MAX) {
                    input_buf[len++] = c;
                    putchar(c);
                }
                fflush(stdout);
                continue;
            }

            input_buf[len] = '\0';
            len = 0;
            putchar('\n');

            if (input_buf[0] == '/') { // parse command
                char command[7];
                char channel[CHANNEL_MAX];
                sscanf(input_buf, "%7s %32s", command, channel);
                if (strncmp(command, "/exit", 5) == 0) { 
                    // logout of server
                    send_logout(); 

                    goto exit;

                } else if (strncmp(command, "/join", 5) == 0) {
                    if (input_buf[5] == ' ') {
                        // join channel
                        if (manage_channels(channel, 'a') == 1) {
                            send_join(channel);
                        } else {
                            printf("Already joined the maximum number");
                            printf(" of channels 20.\n");
                            printf("Please leave one before");
                            printf(" joining another.\n");
                            continue;
                        }
                    } else printf("Invalid usage: /join channel\n");
                } else if (strncmp(command, "/leave", 6) == 0) {
                    if (input_buf[6] == ' ') {
                        // leave channel 
                        if (manage_channels(channel, 'r') == 1) {
                            send_leave(channel);
                            if (strcmp(channel, current_channel) == 0) {
                                printf("Please switch channels or join a");
                                printf(" new channel.\n");
                                can_speak = 0;
                            }
                        } else {
                            printf("Error: can't leave a channel not ");
                            printf("subscribed to: %s\n", channel);
                        }
                    } else printf("Invalid usage: /leave channel\n");
                } else if (strncmp(command, "/list", 5) == 0) {
                    // list channels
                    send_list();
                } else if (strncmp(command, "/who", 4) == 0) {
                    if (input_buf[4] == ' ') {    
                        // list users in channel
                       send_who(channel); 
                    } else printf("Invalid usage: /who channel\n"); 
                } else if (strncmp(command, "/switch", 7) == 0) {
                    if (input_buf[7] == ' ') {
                        if (manage_channels(channel, 's') == 0) {
                            printf("Error: not subscribed to %s", channel);
                        }
                    } else printf("Invalid usage: /switch channel\n");
                } else {
                    printf("*Unknown command\n");
                }
                strcpy(channel, "");
                strcpy(command, "");
            } else { // send say message
                if (can_speak)
                    send_say(input_buf);
                else 
                    printf("Please switch channels or join a new channel.\n");
            }
            strcpy(input_buf, "");

            putchar('>');
            fflush(stdout);
        } else if (FD_ISSET(sockfd, &readfds)) {
            recv_text = recv_packet();
            
            // clear terminal line 
            for (int i = 0; i < SAY_MAX; i++) {
                printf("\b \b");
            }

            // direct the packet to the proper handler
            if (recv_text->txt_type == TXT_SAY) {
                recv_say(recv_text); 
            } else if (recv_text->txt_type == TXT_LIST) {
                recv_list(recv_text);
            } else if (recv_text->txt_type == TXT_WHO) {
                recv_who(recv_text);
            } else if (recv_text->txt_type == TXT_ERROR) {
                recv_error(recv_text);
            } else {
                printf("Unknown message sent from the server.\n");
            }
            putchar('>');
            fflush(stdout);
            // replace user text in input buffer after receiving packet
            for (int i = 0; i < len; i++) {
                putchar(input_buf[i]);
            }
            fflush(stdout);
        }
    }
    
exit:
    exit(EXIT_SUCCESS);
}

// catch the user closing the client program
static void exit_handler(int signum) {
    (void) signum;
    exit(EXIT_SUCCESS);
}

// disconnect the user with atexit
static void disconnect() {
    close(sockfd);
    if (raw) cooked_mode();
    if (logged_in) send_logout();
}

// connect the client to the server
static int connect_to_server() {
    int ret = 0;
    struct sockaddr_in server_addr;
    struct hostent *host; 

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Error: could not create socket.\n");
        ret = -1;
    }

    host = gethostbyname(HOSTNAME);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = *(in_addr_t *) host->h_addr_list[0];
    
    if (connect(sockfd, (struct sockaddr *) &server_addr, 
                                            sizeof(server_addr)) < 0) {
        printf("Error: could not connect to server.\n");
        ret = -1;
    }
    return ret;
}

// make the channels list empty
static void init_channels() {
    for (int i = 0; i < 20; i++) {
        strcpy(channels[i], "");
    }
}

// manage when the channels list when a user 
// adds, switches, or removes a channel
static int manage_channels(char *channel, char flag) {
    // default channel was not found or there was no room in channels.
    int ret = 0; 
    
    switch (flag) {
        case 'a':
            for (int i = 0; i < 20; i++) {
                if (strcmp(channels[i], "") == 0) {
                    strcpy(channels[i], channel); // subscribe to channel
                    strcpy(current_channel, channel); // change to channel
                    num_channels++;
                    if (!can_speak) can_speak = 1;
                    ret = 1; // there was room so channel was added
                    break;
                }
            } 
            break;
        case 's':   
            for (int i = 0; i < 20; i++) {
                if (strcmp(channels[i], channel) == 0) {
                    strcpy(current_channel, channel); // switch to channel
                    if (!can_speak) can_speak = 1;
                    ret = 1; // channel was found in channels
                    break;
                }
            }
            break;
        case 'r':
            for (int i = 0; i < 20; i++) {
                if (strcmp(channels[i], channel) == 0) {
                    strcpy(channels[i], ""); // unsubscribe from channel
                    num_channels--;
                    ret = 1; // channel was found in channels
                    break;
                }
            }
            break;
    }
    return ret;
}

// send a login request packet to the server
static void send_login() {
    struct request_login login;
    memset(&login, 0, sizeof(login));
    login.req_type = REQ_LOGIN;
    strcpy(login.req_username, USERNAME);

    send(sockfd, &login, sizeof(login), 0);
    
    logged_in = 1;
}

// send a logout request packet to the server
static void send_logout() {
    struct request_logout logout;
    memset(&logout, 0, sizeof(logout));
    logout.req_type = REQ_LOGOUT;

    send(sockfd, &logout, sizeof(logout), 0);

    logged_in = 0;
}

// send a join request to the server for the specified channel
static void send_join(char *channel) {
    struct request_join join_init;
    memset(&join_init, 0, sizeof(join_init));
    join_init.req_type = REQ_JOIN;
    strcpy(join_init.req_channel, channel);

    send(sockfd, &join_init, sizeof(join_init), 0);
}

// send a leave request to the server for the specfied channel
static void send_leave(char *channel) {
    struct request_leave leave;
    memset(&leave, 0, sizeof(leave));
    leave.req_type = REQ_LEAVE;
    strcpy(leave.req_channel, channel);

    send(sockfd, &leave, sizeof(leave), 0);
}

// send a list request to the server
static void send_list() {
    struct request_list list;
    memset(&list, 0, sizeof(list));
    list.req_type = REQ_LIST;

    send(sockfd, &list, sizeof(list), 0);
}

// send a who request to the server for the specified channels
static void send_who(char *channel) {
    struct request_who who;
    memset(&who, 0, sizeof(who));
    who.req_type = REQ_WHO;
    strcpy(who.req_channel, channel);

    send(sockfd, &who, sizeof(who), 0);
}

// send a say message packet to the server
static void send_say(char *input) {
    struct request_say msg;
    memset(&msg, 0, sizeof(msg));
    msg.req_type = REQ_SAY;
    strcpy(msg.req_channel, current_channel);
    strcpy(msg.req_text, input);

    send(sockfd, &msg, sizeof(msg), 0);
}

// receive a generic text packet and copy it to the buffer
static struct text* recv_packet() {
    memset(recv_buf, 0, sizeof(recv_buf));
    recv(sockfd, recv_buf, 1024, 0);
    
    return (struct text *) recv_buf;
}

// receive the say text packet from the server and print its contents
static void recv_say(struct text *recv_text) {
    struct text_say *say_text = (struct text_say *) recv_text;
    
    if ((strlen(say_text->txt_channel) <= CHANNEL_MAX) &&
        (strlen(say_text->txt_username) <= USERNAME_MAX) &&
        (strlen(say_text->txt_text) <= SAY_MAX)) {
    
        printf("[%s][%s]: %s\n",    say_text->txt_channel,
                                    say_text->txt_username,
                                    say_text->txt_text);
    }
}

// receive the list text packet from the server and print its contents
static void recv_list(struct text *recv_text) {
    struct text_list *list_text = (struct text_list *) recv_text;
    printf("Existing channels:\n");
    for (int i = 0; i < list_text->txt_nchannels; i++) {
        if (strlen(list_text->txt_channels[i].ch_channel) <= CHANNEL_MAX)
            printf("\t%s\n", list_text->txt_channels[i].ch_channel);
    }
}

// receive the who text packet from the server and print its contents
static void recv_who(struct text *recv_text) {
    struct text_who *who_text = (struct text_who *) recv_text;
    printf("Users on channel %s:\n", who_text->txt_channel);
    for (int i = 0; i < who_text->txt_nusernames; i++) {
        if (strlen(who_text->txt_users[i].us_username) <= USERNAME_MAX)
            printf("\t%s\n", who_text->txt_users[i].us_username);
    }
}

// receive the error text packet from the server and print its contents
static void recv_error(struct text *recv_text) {
    struct text_error *error_text = (struct text_error *) recv_text;
    if (strlen(error_text->txt_error) <= SAY_MAX) 
        printf("%s\n", error_text->txt_error);
}
