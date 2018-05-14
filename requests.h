#ifndef _REQUESTS_H
#define _REQUESTS_H

#include <stdint.h>

#ifdef __cplusplus
#include "requests.pb.h"

extern "C" {
    using namespace pairing_server;
    typedef Request::Operation Operation;
    typedef Response::Status Status;
    typedef Response::Reason Reason;
    typedef TournamentGame::Result Result;
#else
    typedef struct Request Request;
    typedef struct Response Response;
    typedef struct Tournament Tournament;
    typedef struct TournamentPlayer TournamentPlayer;
    typedef struct TournamentGame TournamentGame;

    typedef enum {
        PING = 0,
        TOURNAMENT_CREATE = 1,
        TOURNAMENT_GET = 2,
        TOURNAMENT_EDIT = 3,
        PLAYER_SIGNUP = 4,
        PLAYER_GET = 5,
        PLAYER_EDIT = 6,
        GAME_GET = 7,
        GAME_REGISTER_RESULT = 8,

        TOURNAMENT_GET_ROUND = 9,
        TOURNAMENT_GET_PLAYERS = 10,
        OPERATION_COUNT
    } Operation;
    typedef enum { OK = 0, ERROR = 1 } Status;
    typedef enum {
        UNSPECIFIED = 0,
        INTERNAL_ERROR = 1,
        MISSING_HMAC = 2,
        WRONG_HMAC = 3,
        MISSING_UUID = 4,
        NOT_FOUND = 5,
        INCOMPLETE = 6
    } Reason;
    typedef enum {
        NONE = 0,
        DRAW = 1,
        WHITE_WIN = 2,
        BLACK_WIN = 3,
        WHITE_FORFEIT = 4,
        BLACK_FORFEIT = 5
    } Result;
#endif

    Request *request_read(int socket);
    void request_free(Request *req);
    Tournament *request_tournament(Request *req);
    TournamentGame *request_game(Request *req);
    TournamentPlayer *request_player(Request *req);
    Operation request_operation(Request *req);

    Response *response_new();
    void response_free(Response *resp);
    bool response_write(Response *resp, int socket);
    void response_set_status(Response *resp, uint32_t status);
    void response_add_tournament(Response *resp, Tournament *t);
    void response_add_player(Response *resp, TournamentPlayer *p);
    void response_add_game(Response *resp, TournamentGame *g);
    void response_add_reason(Response *resp, Reason reason);

    bool tournament_complete(Tournament *t);
    bool tournament_identified(Tournament *t);
    bool tournament_authenticated(Tournament *t);
    const char *tournament_name(Tournament *t);
    uint32_t tournament_rounds(Tournament *t);
    const char *tournament_uuid(Tournament *t);
    const char *tournament_hmac(Tournament *t);
    void tournament_set_rounds(Tournament *t, uint32_t rounds);
    void tournament_set_name(Tournament *t, const char *name);
    void tournament_set_uuid(Tournament *t, const char *uuid, const char *digest);

    bool player_complete(TournamentPlayer *p);
    bool player_identified(TournamentPlayer *p);
    bool player_authenticated(TournamentPlayer *p);
    const char *player_name(TournamentPlayer *p);
    Tournament *player_tournament(TournamentPlayer *p);
    uint32_t player_rating(TournamentPlayer *p);
    const char *player_uuid(TournamentPlayer *p);
    const char *player_hmac(TournamentPlayer *p);
    void player_set_name(TournamentPlayer *p, const char *name);
    void player_set_tournament(TournamentPlayer *p, Tournament *t);
    void player_set_rating(TournamentPlayer *p, uint32_t rating);
    void player_set_uuid(TournamentPlayer *p, const char *uuid, const char *digest);

    uint32_t game_round(TournamentGame *g);
    TournamentPlayer *game_white(TournamentGame *g);
    TournamentPlayer *game_black(TournamentGame *g);
    Result game_result(TournamentGame *g);
    const char *game_uuid(TournamentGame *g);
    const char *game_hmac(TournamentGame *g);

#ifdef __cplusplus
}
#endif

#endif
