#include "database.h"
#include "handlers.h"

void handle_ping(Request *req, Response *resp) {
    /* Nothing to do for a ping. */
}

void handle_tournament_create(Request *req, Response *resp) {
    Tournament *t = request_tournament(req);
    bool success;

    if(!t || !tournament_complete(t)) {
        response_set_status(resp, ERROR);
        response_add_reason(resp, INCOMPLETE);
        return;
    }

    success = database_insert_tournament(t);
    if(!success) {
        response_set_status(resp, ERROR);
        response_add_reason(resp, INTERNAL_ERROR);
        return;
    }

    response_add_tournament(resp, t);
}

void handle_tournament_get(Request *req, Response *resp) {
    bool success;
    Tournament *t = request_tournament(req);

    if(!t || !tournament_identified(t)) {
        response_set_status(resp, ERROR);
        response_add_reason(resp, INCOMPLETE);
        return;
    }

    success = database_get_tournament(t);
    if(!success) {
        response_set_status(resp, ERROR);
        response_add_reason(resp, INTERNAL_ERROR);
        return;
    }

    response_add_tournament(resp, t);
}

void handle_player_signup(Request *req, Response *resp) {
    bool success;
    TournamentPlayer *p = request_player(req);
    if(!p || !player_complete(p)) {
        response_set_status(resp, ERROR);
        response_add_reason(resp, INCOMPLETE);
        return;
    }

    success = database_insert_player(p);
    if(!success) {
        response_set_status(resp, ERROR);
        response_add_reason(resp, INTERNAL_ERROR);
        return;
    }

    response_add_player(resp, p);
}
