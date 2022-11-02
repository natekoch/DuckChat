#ifndef SERVER_H
#define SERVER_H

#include "duckchat.h"

#define MAX_USERS 20
#define MAX_CHANNELS 20

typedef struct user {
    char *name;
    char *domain;
    char *port;
    char **channels;
} user;

typedef struct channel {
    char *name;
    int nusers;
    char **users;
} channel;

static void exit_handler(int);

static void free_all();

static void init_channels(void);

static void init_clients(void);

static void free_channels(void);

static void free_clients(void);

static int lookup_client(char *, char *);

static void login_user(char *, char *, struct request*);

static void logout_user(char *, char *);
/*
static int user_join(struct request*);

static int user_leave(struct request*);

static int user_say(struct request*);

static int user_list(struct request*);

static int user_who(struct request*);

static void send_error(char *);
*/
#endif
