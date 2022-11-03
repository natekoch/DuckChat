#ifndef CLIENT_H
#define CLIENT_H

#include "duckchat.h"

static void exit_handler(int);

static void disconnect(void);

static int connect_to_server(void);

static int manage_channels(char *, char);

static void init_channels(void);

static void send_login(void);

static void send_logout(void);

static void send_join(char *);

static void send_leave(char *);

static void send_list(void);

static void send_who(char *);

static void send_say(char *);

static struct text* recv_packet();

static void recv_say(struct text *);

static void recv_list(struct text *);

static void recv_who(struct text *);

static void recv_error(struct text *);

#endif
