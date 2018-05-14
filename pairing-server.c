#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arguments.h"
#include "database.h"
#include "err.h"
#include "handlers.h"
#include "hmac.h"
#include "requests.h"

static bool done = false;
static void done_handler(int signal) {
    done = true;
}

static void usage(bool ok_status, char *progname) {
    fprintf(stderr,
            "Usage: %s [--help] --secret FILE [--listen ADDR] [--port PORT]\n"
            "    --secret (-s) FILE    Path to file containing server secret. Must be mode 0400.\n"
            "    --listen (-l) ADDR    IP-address to listen on for connections. Default 127.0.0.1\n"
            "    --port (-p) PORT      TCP port to listen on. Default 1234.\n"
            "    --help (-h)           Print this message.\n"
            "    --db (-p)             Name of the database to connect to.\n"
            "    --dbuser (-u)         Username use to use for database connection.\n"
            "    --dbpass (-P)         Password use to use for database connection.\n"
        );
    exit(ok_status? 0: 1);
}

static char *read_line(FILE *f) {
    char buf[8192];
    size_t bytes_read;
    size_t bytes_allocated = 1;
    char *line = malloc(1);
    *line = '\0';
    memset(&buf[0], '\0', 8192);

    while(!feof(f)) {
        char *nl;
        bytes_read = fread(&buf[0], sizeof(char), 8192, f);
        if(!bytes_read) {
            if(!feof(f)) {
                err("Error while reading: %s\n", strerror(errno));
                if(line) {
                    free(line);
                    line = NULL;
                }
            }
            break;
        }

        nl = strchr(&buf[0], '\n');
        if(nl) {
            *nl = '\0';
        }

        line = realloc(line, bytes_allocated + bytes_read);
        memset(line + bytes_allocated, '\0', bytes_read);
        bytes_allocated += bytes_read;
        strcat(line, &buf[0]);
    }

    return line;
}

static char* read_secret(char *secret_file) {
    struct stat file_info;
    int ret;
    char *line;

    /* First, make sure that the file exists and has mode 400. */
    ret = stat(secret_file, &file_info);
    if(ret) {
        err("Failed to stat %s: %s\n", secret_file, strerror(errno));
        return NULL;
    }

    if((file_info.st_mode & 0777) != 0400) {
        err("Secret file must have mode 400 (is %o).\n", file_info.st_mode & 0777);
        return NULL;
    }

    FILE *f = fopen(secret_file, "r");
    if(!f) {
        err("Failed to open secret file %s: %s\n", secret_file, strerror(errno));
        return NULL;
    }

    line = read_line(f);
    fclose(f);

    if (!line) {
        err("Failed to read secret from %s\n", secret_file);
    }

    return line;
}

typedef void (*request_handler)(Request *req, Response *resp);
request_handler handlers[OPERATION_COUNT] = {
    handle_ping, /* PING = 0 */
    handle_tournament_create, /* TOURNAMENT_CREATE = 1 */
    handle_tournament_get, /* TOURNAMENT_GET = 2 */
    NULL, /* TOURNAMENT_EDIT = 3 */
    handle_player_signup, /* PLAYER_SIGNUP = 4 */
    NULL, /* PLAYER_GET = 5 */
    NULL, /* PLAYER_EDIT = 6 */
    NULL, /* GAME_GET = 7 */
    NULL, /* GAME_REGISTER_RESULT = 8 */
    NULL, /* TOURNAMENT_GET_ROUND = 9 */
    NULL /* TOURNAMENT_GET_PLAYERS = 10 */
};

int real_main(int argc, char **argv) {
    int s, ret;
    bool success;
    struct sigaction action;
    struct arguments args = {.help=false, .secret_file=NULL};
    char *secret;
    size_t secret_len;
    request_handler handler = NULL;

    args.address.sin_family = AF_INET;
    inet_aton("127.0.0.1", &args.address.sin_addr);
    args.address.sin_port = htons(1234);

    /* Parse arguments. */
    success = parse_arguments(argc, argv, &args);
    if(!success || args.help) {
        usage(args.help, argv[0]);
    }

    secret = read_secret(args.secret_file);
    if(!secret) exit(1);
    secret_len = strlen(secret);
    set_secret(secret, secret_len);

    success = database_connect(args.db, args.dbuser, args.dbpass);
    if(!success) exit(1);

    /* Handle SIGINT (^C from terminal) gracefully by setting the done flag
     * rather than just stopping hard. This lets us finish an ongoing request
     * if the interrupt hits in the middle of processing. But we set
     * SA_RESETHAND, so that if we're hanging somewhere in processing a second
     * SIGINT will just stop the process. */
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND;
    action.sa_handler = done_handler;
    sigaction(SIGINT, &action, NULL);

    /* Set up network socket. */
    s = socket(AF_INET, SOCK_STREAM, 0);
    ret = bind(s, (struct sockaddr *) &args.address, sizeof(struct sockaddr_in));
    ret = listen(s, 10);
    trace("Listening on %s:%d\n", inet_ntoa(args.address.sin_addr), ntohs(args.address.sin_port));
    while(!done) {
        struct sockaddr_in conn_addr;
        socklen_t conn_addr_size;
        int conn = accept(s, (struct sockaddr *) &conn_addr, &conn_addr_size);
        if(conn < 0) {
            if(errno != EINTR) {
                err("Error when accepting connection: %s\n", strerror(errno));
                exit(1);
            }
            continue;
        }

        Request *req = request_read(conn);
        if(!req) {
            shutdown(conn, SHUT_RDWR);
            close(conn);
            continue;
        }

        Response *resp = response_new();

        trace("Got request with operation %ld\n", request_operation(req));

        /* TODO: Server logic goes here.
         * !: Possibly log address of incoming connection?
         * 2: Read incoming request from socket into object.
         * 3: Handle request and write response to socket.
         */
        handler = handlers[request_operation(req)];

        if(handler) {
            handler(req, resp);
        }

        response_write(resp, conn);

        request_free(req);
        response_free(resp);

        shutdown(conn, SHUT_RDWR);
        close(conn);
    }

    return 0;
}
