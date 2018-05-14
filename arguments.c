#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "arguments.h"
#include "err.h"

static bool is(char *a, char *b) {
    return !strcmp(a, b);
}

#define READ_ARG(arg_name) \
    ({i++; \
    if(i >= argc) { \
        ok = false; \
        err("Missing argument to option " arg_name "\n"); \
        break; \
    } argv[i];})
bool parse_arguments(int argc, char **argv, struct arguments *args) {
    bool ok = true, seen_secret = false;
    int i;

    for(i = 1; i < argc; i++) {
        char *arg = argv[i];
        if(is(arg, "--help") || is(arg, "-h")) {
            args->help = true;
        }
        else if(is(arg, "--secret") || is(arg, "-s")) {
            seen_secret = true;
            args->secret_file = READ_ARG("--secret");
        }
        else if(is(arg, "--listen") || is(arg, "-l")) {
            char *listen = READ_ARG("--listen");
            struct sockaddr_in addr;
            int ret = inet_aton(listen, &addr.sin_addr);

            if(!ret) {
                ok = false;
                err("Invalid listen address %s\n", listen);
                continue;
            }

            args->address.sin_addr = addr.sin_addr;
        }
        else if(is(arg, "--port") || is(arg, "-p")) {
            char *port_str = READ_ARG("--port");
            char *end = NULL;
            long int port = strtol(port_str, &end, 10);

            /* The string is invalid if it's empty (!*port_str) or if the
             * number prefix is terminated by a non-zero character (*end). */
            if(!*port_str || *end) {
                ok = false;
                err("Invalid port number %s\n", port_str);
                continue;
            }

            if(port <= 0 || port > 65535) {
                ok = false;
                err("Invalid port number %ld\n", port);
                continue;
            }

            args->address.sin_port = htons(port);
        }
        else if(is(arg, "--db") || is(arg, "-d")) {
            args->db = READ_ARG("--db");
        }
        else if(is(arg, "--dbuser") || is(arg, "-u")) {
            args->dbuser = READ_ARG("--db");
        }
        else if(is(arg, "--dbpass") || is(arg, "-P")) {
            args->dbpass = READ_ARG("--db");
        }
        else {
            err("Unknown option %s\n", arg);
            ok = false;
        }
    }

    if(!seen_secret)
        err("Missing required argument --secret\n");

    return ok && seen_secret;
}
