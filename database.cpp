#include <arpa/inet.h>

#include "database.h"

using namespace pairing_server;

DatabaseError::DatabaseError(const char *dbmsg) : msg(dbmsg) {}

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
    }

    prepare("insert_tournament",
            "INSERT INTO tournament(name, rounds) VALUES ($1, $2) RETURNING uuid", 2);
    prepare("get_tournament",
            "SELECT name, rounds FROM tournament WHERE uuid = $1", 1);
    prepare("insert_player",
            "INSERT INTO tournament_player(player_name, rating, tournament)\n"
            "SELECT $1, $2, id FROM tournament WHERE uuid = $3\n"
            "RETURNING uuid", 3);
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
    PGresult *res = execute("get_tournament", 1, &values[0], &formats[0], &lengths[0], 1);
    /* TODO: Assert that exactly one row is returned. */
    t->set_rounds(ntohl(*(uint32_t *) PQgetvalue(res, 0, PQfnumber(res, "rounds"))));
    t->set_name(PQgetvalue(res, 0, PQfnumber(res, "name")));
    PQclear(res);
}

void Database::insertTournament(Tournament *t) {
    uint32_t netRounds = htonl(t->rounds());
    const char *values[] = {t->name().c_str(), (char *) &netRounds};
    const int formats[] = {0, 1};
    const int lengths[] = {0, sizeof(uint32_t)};
    PGresult *res = execute("insert_tournament", 2, &values[0], &formats[0], &lengths[0], 1);
    /* TODO: Assert that a row is returned. */
    t->mutable_id()->set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")));
    PQclear(res);
}

void Database::insertPlayer(TournamentPlayer *p) {
    uint32_t netRating = htonl(p->rating());
    const char *values[] = {p->name().c_str(), (char *) &netRating,
        p->tournament().id().uuid().c_str()};
    const int formats[] = {0, 1, 1};
    const int lengths[] = {0, sizeof(uint32_t), 16};
    PGresult *res = execute("insert_player", 3, &values[0], &formats[0], &lengths[0], 1);
    /* TODO: Make sure we actually get a row back. */
    p->mutable_id()->set_uuid(PQgetvalue(res, 0, PQfnumber(res, "uuid")));
    PQclear(res);
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
