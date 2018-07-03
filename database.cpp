#include <arpa/inet.h>

#include "database.h"

using namespace pairing_server;

uint32_t intify(const char *x) {
    return ntohl(*(uint32_t *) x);
}

Database::Database(const char *dbname, const char *user, const char *password,
            const char *host) {
    const char *keys[] = {"hostaddr", "dbname", "user", "password", NULL};
    const char *values[] = {host, dbname, user, password, NULL};
    db = PQconnectdbParams(&keys[0], &values[0], 0);

    if(!db || PQstatus(db) == CONNECTION_BAD) {
        DatabaseError e = DatabaseError(PQerrorMessage(db));
        if(db) {
            PQfinish(db);
            db = NULL;
        }
        throw e;
    }

    prepare("get_tournament",
            "SELECT name, rounds FROM tournament WHERE uuid = $1", 1);
    prepare("next_round",
            "SELECT MAX(round) + 1 AS round FROM game WHERE tournament = 1", 1);
    prepare("players",
            "SELECT player_name, rating, p.uuid AS uuid\n"
            "FROM player p INNER JOIN tournament t ON p.tournament = t.id\n"
            "WHERE t.uuid = $1", 1);
    prepare("games",
           "SELECT w.player_name AS white_name, w.rating AS white_rating, w.uuid AS white_uuid,\n"
           "       b.player_name AS black_name, b.rating AS black_rating, b.uuid AS black_uuid,\n"
           "       result, round, g.uuid AS uuid\n"
           "FROM game g INNER JOIN player w ON white = w.id\n"
           "            LEFT  JOIN player b ON black = b.id\n"
           "WHERE g.uuid = $1", 1);

    prepare("insert_tournament",
            "INSERT INTO tournament(name, rounds) VALUES ($1, $2) RETURNING uuid", 2);
    prepare("insert_player",
            "INSERT INTO player(player_name, rating, tournament)\n"
            "SELECT $1, $2, id FROM tournament WHERE uuid = $3\n"
            "RETURNING uuid", 3);
    prepare("insert_game",
            "INSERT INTO game(tournament, white, black)\n"
            "SELECT t.id, w.id, b.id\n"
            "FROM tournament t, player w, player b\n"
            "WHERE t.uuid = $1 AND w.uuid = $2 AND b.uuid = $3\n"
            "RETURNING uuid", 3);
    prepare("insert_game_with_result",
            "INSERT INTO game(tournament, white, black, result)\n"
            "SELECT t.id, w.id, b.id, $4\n"
            "FROM tournament t, player w, player b\n"
            "WHERE t.uuid = $1 AND w.uuid = $2 AND b.uuid = $3\n"
            "RETURNING uuid", 3);

    prepare("register_result",
            "UPDATE game SET result = $1 WHERE uuid = $2", 2);
}

Database::~Database() {
    if(db) {
        PQfinish(db);
        db = NULL;
    }
}

void Database::sqlDo(const char *sql) {
    PGresult *res = PQexec(db, sql);
    ExecStatusType status = PQresultStatus(res);
    PQclear(res);

    if(!res || status != PGRES_COMMAND_OK) {
        throw DatabaseError(PQerrorMessage(db));
    }
}

void Database::begin() { sqlDo("BEGIN"); }
void Database::commit() { sqlDo("COMMIT"); }
void Database::rollback() { sqlDo("ROLLBACK"); }

void Database::getTournament(Tournament *t) {
    const char *values[] = {t->id().uuid().c_str()};
    const int formats[] = {1};
    const int lengths[] = {16};
    PGresult *res = execute("get_tournament", 1, &values[0], &lengths[0], &formats[0], 1);
    /* TODO: Assert that exactly one row is returned. */
    t->set_rounds(ntohl(*(uint32_t *) PQgetvalue(res, 0, PQfnumber(res, "rounds"))));
    t->set_name(PQgetvalue(res, 0, PQfnumber(res, "name")));
    PQclear(res);
}

int Database::nextRound(const Identification *id) {
    const char *values[] = {id->uuid().c_str()};
    const int formats[] = {1};
    const int lengths[] = {16};
    PGresult *res = execute("next_round", 1, &values[0], &lengths[0], &formats[0], 1);
    int round = PQntuples(res) > 0?
        intify(PQgetvalue(res, 0, PQfnumber(res, "round"))):
        1;
    PQclear(res);
    return round;
}

std::vector<Player> Database::tournamentPlayers(const Identification *id) {
    const char *values[] = {id->uuid().c_str()};
    const int formats[] = {1};
    const int lengths[] = {16};
    PGresult *res = execute("players", 1, &values[0], &lengths[0], &formats[0], 1);
    std::vector<Player> vec(PQntuples(res));
    for(int i = 0; i < PQntuples(res); i++) {
        vec[i] = playerFromRow(res, i);
    }
    PQclear(res);
    return vec;
}

std::vector<Game> Database::tournamentGames(const Identification *id) {
    const char *values[] = {id->uuid().c_str()};
    const int formats[] = {1};
    const int lengths[] = {16};
    PGresult *res = execute("games", 1, &values[0], &lengths[0], &formats[0], 1);
    std::vector<Game> vec(10);
    for(int i = 0; i < PQntuples(res); i++) {
        vec[i] = Game();
        vec[i].mutable_id()->set_uuid(PQgetvalue(res, i, PQfnumber(res, "uuid")));
        vec[i].set_round(intify(PQgetvalue(res, i, PQfnumber(res, "round"))));
        if(!PQgetisnull(res, i, PQfnumber(res, "result"))) {
            vec[i].set_result(static_cast<Result>(intify(PQgetvalue(res, i, PQfnumber(res, "result")))));
        }
        *(vec[i].mutable_white()) = playerFromRow(res, i, "white_name", "white_rating", "white_uuid");
        *(vec[i].mutable_black()) = playerFromRow(res, i, "black_name", "black_rating", "black_uuid");
    }
    PQclear(res);
    return vec;
}

Identification Database::insertTournament(const Tournament *t) {
    uint32_t netRounds = htonl(t->rounds());
    const char *values[] = {t->name().c_str(), (char *) &netRounds};
    const int formats[] = {0, 1};
    const int lengths[] = {0, sizeof(uint32_t)};
    PGresult *res = execute("insert_tournament", 2, &values[0], &lengths[0], &formats[0], 1);
    /* TODO: Assert that a row is returned. */
    Identification ident;
    ident.set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")));
    PQclear(res);
    return ident;
}

Identification Database::insertPlayer(const Player *p) {
    uint32_t netRating = htonl(p->rating());
    const char *values[] = {p->name().c_str(), (char *) &netRating,
        p->tournament().id().uuid().c_str()};
    const int formats[] = {0, 1, 1};
    const int lengths[] = {0, sizeof(uint32_t), 16};
    PGresult *res = execute("insert_player", 3, &values[0], &lengths[0], &formats[0], 1);
    /* TODO: Make sure we actually get a row back. */
    Identification ident;
    ident.set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")));
    PQclear(res);
    return ident;
}

Identification Database::insertGame(const Game *g) {
    PGresult *res;
    /* Since the only difference between the two prepared statements here is
     * an extra argument at the end of insert_game_with_result, we can
     * allocate a single set of data for both invocations. In the case of
     * insert_game, the last element simply won't be read.
     */
    const char *values[] = {g->tournament().id().uuid().c_str(),
        g->white().id().uuid().c_str(),
        g->black().id().uuid().c_str(),
        0};
    const int lengths[] = {16, 16, 16, sizeof(uint32_t)};
    const int formats[] = {1, 1, 1, 1};

    if(g->result() > 0) {
        uint32_t netRound = htonl(g->round());
        values[3] = (char *) &netRound;
        res = execute("insert_game_with_result", 4, &values[0], &lengths[0], &formats[0], 1);
    }
    else {
        res = execute("insert_game", 3, &values[0], &lengths[0], &formats[0], 1);
    }

    Identification id;
    id.set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")));
    return id;
}

void Database::registerResult(const Identification &gameId, Result result) {
    uint32_t netResult = htonl(result);
    const char *values[] = {(char *) &netResult, gameId.uuid().c_str()};
    const int formats[] = {0, 1};
    const int lengths[] = {0, sizeof(uint32_t)};
    PGresult *res = execute("register_result", 2, &values[0], &lengths[0], &formats[0], 1);
}

/* Private helper methods: */
void Database::prepare(const char *name, const char *sql, int count) {
    PGresult *res = PQprepare(db, name, sql, count, NULL);
    if(!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if(res) PQclear(res);
        throw DatabaseError(PQerrorMessage(db));
    }
}

PGresult *Database::execute(const char *stmt, int count, const char **values,
        const int *lengths, const int *formats, int resultFormat) {
    PGresult *res = PQexecPrepared(db, stmt, count, values, lengths, formats,
            resultFormat);
    if(!res || (PQresultStatus(res) != PGRES_COMMAND_OK &&
                PQresultStatus(res) != PGRES_TUPLES_OK)) {
        if(res) PQclear(res);
        throw DatabaseError(PQerrorMessage(db));
    }
    return res;
}

Player Database::playerFromRow(PGresult *res, int i, const char *name_col, const char *rating_col, const char *uuid_col) {
    Player p;
    p.mutable_id()->set_uuid(PQgetvalue(res, i, PQfnumber(res, uuid_col)));
    p.set_name(PQgetvalue(res, i, PQfnumber(res, name_col)));
    p.set_rating(intify(PQgetvalue(res, i, PQfnumber(res, rating_col))));
    return p;
}
