#include <arpa/inet.h>

#include "database.h"

using namespace pairing_server;

uint32_t intify(const char *x) {
    return ntohl(*(uint32_t *) x);
}

Database::Database() {}

Database::Database(const char *dbname, const char *user, const char *password, const char *host) :
    dbname(dbname), user(user), password(password), host(host) {}

void Database::connect() {
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
            "SELECT MAX(round) + 1 AS round\n"
            "FROM game INNER JOIN tournament t ON tournament = t.id\n"
            "WHERE t.uuid = $1", 1);
    prepare("get_player",
            "SELECT p.uuid AS uuid, player_name, rating, withdrawn, expelled,\n"
            "   t.name AS tournament_name, t.uuid AS tournament_uuid, rounds\n"
            "FROM player p INNER JOIN tournament t ON tournament = t.id\n"
            "WHERE p.uuid = $1", 1);
    prepare("players",
            "SELECT player_name, rating, p.uuid AS uuid\n"
            "FROM player p INNER JOIN tournament t ON p.tournament = t.id\n"
            "WHERE t.uuid = $1", 1);
    prepare("get_game",
            "SELECT white, black, round, result FROM game WHERE uuid = $1", 1);
    prepare("games",
           "SELECT w.player_name AS white_name, w.rating AS white_rating, w.uuid AS white_uuid,\n"
           "       b.player_name AS black_name, b.rating AS black_rating, b.uuid AS black_uuid,\n"
           "       result, round, g.uuid AS uuid\n"
           "FROM game g INNER JOIN tournament t ON tournament = t.id\n"
           "            INNER JOIN player w ON white = w.id\n"
           "            LEFT  JOIN player b ON black = b.id\n"
           "WHERE t.uuid = $1", 1);

    prepare("insert_tournament",
            "INSERT INTO tournament(name, rounds) VALUES ($1, $2) RETURNING uuid", 2);
    prepare("insert_player",
            "INSERT INTO player(player_name, rating, tournament)\n"
            "SELECT $1, $2, id FROM tournament WHERE uuid = $3\n"
            "RETURNING uuid", 3);
    prepare("insert_game",
            "INSERT INTO game(tournament, white, black, round)\n"
            "SELECT t.id, w.id, b.id, $4\n"
            "FROM tournament t, player w, player b\n"
            "WHERE t.uuid = $1 AND w.uuid = $2 AND b.uuid = $3\n"
            "RETURNING uuid", 4);
    prepare("insert_game_without_black",
            "INSERT INTO game(tournament, white, round)\n"
            "SELECT t.id, w.id, $3\n"
            "FROM tournament t, player w\n"
            "WHERE t.uuid = $1 AND w.uuid = $2\n"
            "RETURNING uuid", 3);
    prepare("insert_game_with_result",
            "INSERT INTO game(tournament, white, black, round, result)\n"
            "SELECT t.id, w.id, b.id, $4, $5\n"
            "FROM tournament t, player w, player b\n"
            "WHERE t.uuid = $1 AND w.uuid = $2 AND b.uuid = $3\n"
            "RETURNING uuid", 5);
    prepare("insert_game_with_result_without_black",
            "INSERT INTO game(tournament, white, round, result)\n"
            "SELECT t.id, w.id, $3, $4\n"
            "FROM tournament t, player w\n"
            "WHERE t.uuid = $1 AND w.uuid = $2\n"
            "RETURNING uuid", 4);

    /* XXX: Consider forcing register_result to only work on games with no
     * result (by adding WHERE result IS NULL) and adding a separate query to
     * update a result. */
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

bool Database::getTournament(Tournament *t) {
    const char *values[] = {t->id().uuid().c_str()};
    const int formats[] = {1};
    const int lengths[] = {16};
    bool found = false;
    PGresult *res = execute("get_tournament", 1, &values[0], &lengths[0], &formats[0], 1, 0, 1);

    if(PQntuples(res) > 0) {
        found = true;
        t->set_rounds(intify(PQgetvalue(res, 0, PQfnumber(res, "rounds"))));
        t->set_name(PQgetvalue(res, 0, PQfnumber(res, "name")));
    }

    PQclear(res);
    return found;
}

int Database::nextRound(const Identification *id) {
    const char *values[] = {id->uuid().c_str()};
    const int formats[] = {1};
    const int lengths[] = {16};
    PGresult *res = execute("next_round", 1, &values[0], &lengths[0], &formats[0], 1, 1, 1);
    int round = PQgetisnull(res, 0, PQfnumber(res, "round"))?
        1:
        intify(PQgetvalue(res, 0, PQfnumber(res, "round")));
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
    std::vector<Game> vec(PQntuples(res));
    for(int i = 0; i < PQntuples(res); i++) {
        vec[i] = Game();
        vec[i].mutable_id()->set_uuid(PQgetvalue(res, i, PQfnumber(res, "uuid")), 16);
        vec[i].set_round(intify(PQgetvalue(res, i, PQfnumber(res, "round"))));
        if(!PQgetisnull(res, i, PQfnumber(res, "result"))) {
            vec[i].set_result(static_cast<Result>(intify(PQgetvalue(res, i, PQfnumber(res, "result")))));
        }
        *(vec[i].mutable_white()) = playerFromRow(res, i, "white_name", "white_rating", "white_uuid");
        if(!PQgetisnull(res, i, PQfnumber(res, "black_name")))
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
    PGresult *res = execute("insert_tournament", 2, &values[0], &lengths[0], &formats[0], 1, 1, 1);
    /* TODO: Assert that a row is returned. */
    Identification ident;
    ident.set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")), 16);
    PQclear(res);
    return ident;
}

bool Database::getPlayer(Player *p) {
    const char *values[] = {p->id().uuid().c_str()};
    const int lengths[] = {16};
    const int formats[] = {1};
    PGresult *res = execute("get_player", 1, &values[0], &lengths[0], &formats[0], 1, 0, 1);
    bool found = false;
    if(PQntuples(res) > 0) {
        found = true;
        *p = playerFromRow(res, 0);
        p->mutable_tournament()->set_name(PQgetvalue(res, 0, PQfnumber(res, "tournament_name")));
        p->mutable_tournament()->set_rounds(intify(PQgetvalue(res, 0, PQfnumber(res, "rounds"))));
        p->mutable_tournament()->mutable_id()->set_uuid(PQgetvalue(res, 0, PQfnumber(res, "tournament_uuid")));
    }
    PQclear(res);
    return found;
}

Identification Database::insertPlayer(const Player *p) {
    uint32_t netRating = htonl(p->rating());
    const char *values[] = {p->name().c_str(), (char *) &netRating,
        p->tournament().id().uuid().c_str()};
    const int formats[] = {0, 1, 1};
    const int lengths[] = {0, sizeof(uint32_t), 16};
    PGresult *res = execute("insert_player", 3, &values[0], &lengths[0], &formats[0], 1, 1, 1);
    /* TODO: Make sure we actually get a row back. */
    Identification ident;
    ident.set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")), 16);
    PQclear(res);
    return ident;
}

Identification Database::insertGame(const Game *g) {
    PGresult *res;
    const char *values[] = {g->tournament().id().uuid().c_str(),
        g->white().id().uuid().c_str(),
        NULL,
        NULL,
        NULL};
    int lengths[] = {16, 16, 0, 0, 0};
    int formats[] = {1, 1, 1, 1, 1};
    int params = 2;

    if(g->has_black()) {
        values[params] = g->black().id().uuid().c_str();
        lengths[params] = 16;
        params++;
    }

    uint32_t netRound = htonl(g->round());
    values[params] = (char *) &netRound;
    lengths[params++] = sizeof(uint32_t);

    uint32_t netResult; // Declared here so that it has same scope as the function.
    if(g->result() > 0) {
        netResult = htonl(g->result());
        values[params] = (char *) &netResult;
        lengths[params++] = sizeof(uint32_t);
    }

    const char *query = g->result() > 0 && g->has_black()? "insert_game_with_result":
                        g->result() > 0? "insert_game_with_result_without_black":
                        g->has_black()? "insert_game":
                                        "insert_game_without_black";
    res = execute(query, params, &values[0], &lengths[0], &formats[0], 1, 1, 1);

    Identification id;
    id.set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")), 16);
    PQclear(res);
    return id;
}

void Database::registerResult(const Identification &gameId, Result result) {
    uint32_t netResult = htonl(result);
    const char *values[] = {(char *) &netResult, gameId.uuid().c_str()};
    const int formats[] = {0, 1};
    const int lengths[] = {0, sizeof(uint32_t)};
    PGresult *res = execute("register_result", 2, &values[0], &lengths[0], &formats[0], 1, 1, 1);
    PQclear(res);
}

/* Private helper methods: */
void Database::prepare(const char *name, const char *sql, int count) {
    PGresult *res = PQprepare(db, name, sql, count, NULL);
    if(!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if(res) PQclear(res);
        throw DatabaseError(PQerrorMessage(db));
    }
    PQclear(res);
}

PGresult *Database::execute(const char *stmt, int count, const char **values,
        const int *lengths, const int *formats, int resultFormat,
        int minRows, int maxRows) {
    PGresult *res = PQexecPrepared(db, stmt, count, values, lengths, formats,
            resultFormat);
    if(!res || (PQresultStatus(res) != PGRES_COMMAND_OK &&
                PQresultStatus(res) != PGRES_TUPLES_OK)) {
        if(res) PQclear(res);
        throw DatabaseError(PQerrorMessage(db));
    }
    int tuples = PQntuples(res);
    if(tuples < minRows) {
        char msgbuf[100];
        snprintf(&msgbuf[0], 100, "Got %d rows, which is less than %d.", tuples, minRows);
        throw DatabaseError(msgbuf);
    }
    if(maxRows > 0 && tuples > maxRows) {
        char msgbuf[100];
        snprintf(&msgbuf[0], 100, "Got %d rows, which is more than %d.", tuples, maxRows);
        throw DatabaseError(msgbuf);
    }
    return res;
}

Player Database::playerFromRow(PGresult *res, int i, const char *name_col,
        const char *rating_col, const char *uuid_col, const char *withdrawn_col,
        const char *expelled_col) {
    Player p;
    int fnum;

    if((fnum = PQfnumber(res, uuid_col)) >= 0)
        p.mutable_id()->set_uuid(PQgetvalue(res, i, fnum), 16);
    else
        throw DatabaseError("UUID column missing in row.");
    if((fnum = PQfnumber(res, name_col)) >= 0)
        p.set_name(PQgetvalue(res, i, fnum));
    else
        throw DatabaseError("Name column missing in row.");
    if((fnum = PQfnumber(res, rating_col)) >= 0)
        p.set_rating(intify(PQgetvalue(res, i, fnum)));
    else
        throw DatabaseError("Rating column missing in row.");

    // Withdrawn and expelled are optional.
    if((fnum = PQfnumber(res, withdrawn_col)) >= 0) {
        p.set_withdrawn(intify(PQgetvalue(res, i, fnum)));
    }
    if((fnum = PQfnumber(res, expelled_col)) >= 0) {
        p.set_expelled(intify(PQgetvalue(res, i, fnum)));
    }
    return p;
}
