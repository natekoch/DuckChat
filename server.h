#ifndef SERVER_H
#define SERVER_H

#include "duckchat.h"
typedef struct user {
    char name[USERNAME_MAX];
    char domain[UNIX_PATH_MAX];
    char port[5];
    char channels[20][CHANNEL_MAX];
} user;

static int add_user(char *, char *);

#endif