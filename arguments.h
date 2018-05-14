#ifndef _ARGUMENTS_H
#define _ARGUMENTS_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>

struct arguments {
    bool help;
    char *secret_file;
    char *db;
    char *dbuser;
    char *dbpass;
    struct sockaddr_in address;
};

bool parse_arguments(int argc, char **argv, struct arguments *args);

#endif
