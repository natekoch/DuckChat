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
    struct sockaddr_in *address;
} user;

typedef struct channel {
    char *name;
    int nusers;
    int *user_indecies;
} channel;

static void exit_handler(int);

static void free_all();

static void init_channels(void);

static void init_clients(void);

static void free_channels(void);

static void free_clients(void);

static int lookup_client(char *, char *);

static void login_user(char *, char *, struct request*, struct sockaddr_in*);

static void logout_user(char *, char *);

static int user_join(char *, char *, struct request*);

static int user_leave(char *, char *, struct request*);
/*
static int user_say(char *, char *, struct request*);

static int user_list(char *, char *, struct request*);

static int user_who(char *, char *, struct request*);
*/
static void send_error(char *, char *, char *);

#endif
