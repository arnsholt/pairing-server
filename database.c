#include <arpa/inet.h>
#include <postgresql/libpq-fe.h>
#include <stdbool.h>
#include <uuid/uuid.h>

#include "database.h"
#include "err.h"
#include "hmac.h"

static PGconn *db = NULL;

static bool prepare(char *name, char *sql, int params) {
    PGresult *res = PQprepare(db, name, sql, params, NULL);
    if(!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        err("Error creating prepared statement (%s): %s",
                name, PQerrorMessage(db));
    }
}

bool database_connect(char *dbname, char *username, char *password) {
    const char *keys[] = {"hostaddr", "dbname", "user", "password", NULL};
    const char *values[] = {"127.0.0.1", dbname, username, password, NULL};
    PGresult *res;
    bool success = true;

    db = PQconnectdbParams(&keys[0], &values[0], 0);

    if(!db || PQstatus(db) == CONNECTION_BAD) {
        err("Error connecting to the database: %s", PQerrorMessage(db));
        return false;
    }

#define PREPARE(n, s, c) success = success && prepare(n, s, c)
    /* TODO: Set up prepared statements. */
    PREPARE("insert_tournament",
            "INSERT INTO tournament(name, rounds) VALUES ($1, $2) RETURNING uuid", 2);
    PREPARE("get_tournament",
            "SELECT name, rounds FROM tournament WHERE uuid = $1", 1);
    PREPARE("insert_player",
            "INSERT INTO tournament_player(player_name, rating, tournament)\n"
            "SELECT $1, $2, id FROM tournament WHERE uuid = $3\n"
            "RETURNING uuid", 3);
#undef PREPARE

    return success;
}

bool database_insert_tournament(Tournament *t) {
    uint32_t netRounds = htonl(tournament_rounds(t));
    const char *paramValues[] = {tournament_name(t), (char *) &netRounds};
    const int paramFormats[] = {0, 1};
    const int paramLengths[] = {0, sizeof(uint32_t)};
    char uuid[37];
    PGresult *result = PQexecPrepared(db, "insert_tournament", 2,
            &paramValues[0], &paramLengths[0], &paramFormats[0], 1);

    if(!result || PQresultStatus(result) != PGRES_TUPLES_OK) {
        err("Error inserting tournament: %s", PQerrorMessage(db));
        if(result) PQclear(result);
        return false;
    }

    uuid_unparse_lower(PQgetvalue(result, 0, PQfnumber(result, "uuid")), &uuid[0]);

    tournament_set_uuid(t, &uuid[0], NULL);
    PQclear(result);
    return true;
}

bool database_get_tournament(Tournament *t) {
    char uuid[16];
    const char *paramValues[] = {&uuid[0]};
    const int paramFormats[] = {1};
    const int paramLengths[] = {16};
    PGresult *result;

    uuid_parse(tournament_uuid(t), uuid);

    result = PQexecPrepared(db, "get_tournament", 1,
            &paramValues[0], &paramLengths[0], &paramFormats[0], 1);
    if(!result || PQresultStatus(result) != PGRES_TUPLES_OK) {
        err("Error inserting tournament: %s", PQerrorMessage(db));
        if(result) PQclear(result);
        return false;
    }

    tournament_set_name(t, PQgetvalue(result, 0, PQfnumber(result, "player_name")));
    tournament_set_rounds(t, ntohl(*(uint32_t *) PQgetvalue(result, 0, PQfnumber(result, "player_name"))));
    PQclear(result);
    return true;
}

bool database_insert_player(TournamentPlayer *p) {
    char uuid[16];
    char uuid_string[37];
    uint32_t netRating = htonl(player_rating(p));
    const char *paramValues[] = {player_name(p), (char *) &netRating, &uuid[0]};
    const int paramFormats[] = {0, 1, 1};
    const int paramLengths[] = {0, sizeof(uint32_t), 16};
    PGresult *result;

    uuid_parse(tournament_uuid(player_tournament(p)), uuid);

    result = PQexecPrepared(db, "insert_player", 3,
            &paramValues[0], &paramLengths[0], &paramFormats[0], 1);
    if(!result || PQresultStatus(result) != PGRES_TUPLES_OK) {
        err("Error inserting tournament: %s", PQerrorMessage(db));
        if(result) PQclear(result);
        return false;
    }

    uuid_unparse_lower(PQgetvalue(result, 0, PQfnumber(result, "uuid")), &uuid_string[0]);
    player_set_uuid(p, &uuid_string[0], NULL);
    PQclear(result);
    return true;
}
